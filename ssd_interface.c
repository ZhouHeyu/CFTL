/* 
 * Contributors: Youngjae Kim (youkim@cse.psu.edu)
 *               Aayush Gupta (axg354@cse.psu.edu_
 *   
 * In case if you have any doubts or questions, kindly write to: youkim@cse.psu.edu 
 *
 * This source plays a role as bridiging disksim and flash simulator. 
 * 
 * Request processing flow: 
 *
 *  1. Request is sent to the simple flash device module. 
 *  2. This interface determines FTL type. Then, it sends the request
 *     to the lower layer according to the FTL type. 
 *  3. It returns total time taken for processing the request in the flash. 
 *
 */

#include "ssd_interface.h"
#include "disksim_global.h"
#include "dftl.h"
#include "stdio.h"


extern int merge_switch_num;
extern int merge_partial_num;
extern int merge_full_num;
int old_merge_switch_num = 0;
int old_merge_partial_num = 0;
int old_merge_full_num= 0;
int old_flash_gc_read_num = 0;
int old_flash_erase_num = 0;
int req_count_num = 1;
int cache_hit, rqst_cnt;

int cache_cmt_hit;
int cache_scmt_hit;
int cache_slcmt_hit;

int flag1 = 1;
int count = 0;
int update_flag = 0;//shzb:用于判断连续缓存中的部分映射项更新标志是否置零
int translation_read_num = 0;
int translation_write_num = 0;


int MLC_page_num_for_2nd_map_table;
int nand_SLC_page_num;

//shzb:odftl
int indexofseq = 0,indexofarr = 0;
int indexold = 0,indexnew = 0,index2nd=0;
int maxentry=0,MC=0;//分别为出现最多的项的值和出现的次数
 
int in_cache_count;
//du:自适应
#define itemcount_threshold 1  //trace的阈值

int write_cnt=0;
int read_cnt=0;
int request_cnt=0;
double write_ratio=0;
double read_ratio=0;

int total_request_size=0;
int itemcount=0;
double average_request_size=0;

#define CACHE_MAX_ENTRIES 300

#define THRESHOLD 2
#define NUM_ENTRIES_PER_TIME 8
//duchenjie:判断MC
#define Threshold 100
//翻译页的初始化
#define init_size 32
#define increment 8

int sequential_count=0;
int big_request_entry = 0;
int big_request_count = 0;


int MAP_REAL_MAX_ENTRIES=0;
int MAP_GHOST_MAX_ENTRIES=0;
int MAP_SEQ_MAX_ENTRIES=0; 
int MAP_SECOND_MAX_ENTRIES=0; 
int *real_arr= NULL;

//int *ghost_arr=NULL;
int *seq_arr= NULL;
int *second_arr=NULL;

int cache_arr[CACHE_MAX_ENTRIES];
/***********************************************************************
  Variables for statistics    
 ***********************************************************************/
unsigned int cnt_read = 0;
unsigned int cnt_write = 0;
unsigned int cnt_delete = 0;
unsigned int cnt_evict_from_flash = 0;
unsigned int cnt_evict_into_disk = 0;
unsigned int cnt_fetch_miss_from_disk = 0;
unsigned int cnt_fetch_miss_into_flash = 0;

double sum_of_queue_time = 0.0;
double sum_of_service_time = 0.0;
double sum_of_response_time = 0.0;
unsigned int total_num_of_req = 0;

/***********************************************************************
  Mapping table
 ***********************************************************************/
 int sys_time=0;
int real_min = -1;
int real_max = 0;
int ghost_min=-1;
/*********
 *  CPFTL
 * *******/
int second_max=0;
int second_min=-1;
/*
int second_min = -1;
int second_max = 0;
//duchenjie:slcmt
int block_min = -1;
int block_max = 0;
struct lru *block;
//memset(blk,0xFF,INIT_SIZE*sizeof(struct lru));
int block_index = 0;
int block_entry_num = 0;
int blocksize = init_size;//LRU块列表中初始块的个数*/

/***********************************************************************
 *    author: zhoujie
 *  封装 callFsim的代码函数
 ***********************************************************************/
void SecnoToPageno(int secno,int scount,int *blkno,int *bcount,int flash_flag);
/***********************************************************************
 *  author :zhoujie       
 * SDFTL 代码执行的内部封装函数
 ***********************************************************************/
void Hit_CMT_Entry(int blkno,int operation);
void CMT_Is_Full();
void Hit_SCMT_Entry(int blkno,int operation);
void Hit_SL_CMT_Entry(int blkno,int operation);
void pre_load_entry_into_SCMT(int *pageno,int *req_size,int operation);
void req_Entry_Miss_SDFTL(int blkno,int operation);
void SDFTL_Scheme(int *pageno,int *req_size,int operation,int flash_flag);


/***********************************************************************
 *             author:zhoujie       DFTL  主函数逻辑实现
 ***********************************************************************/ 
void DFTL_Scheme(int *pageno,int *req_size,int operation,int flash_flag);
/********************************************************
 *         author:zhoujie     DFTL 封装的相关函数
 * *******************************************************/
void DFTL_init_arr();
void DFTL_Ghost_CMT_Full();
void DFTL_Real_CMT_Full();
void DFTL_Hit_Ghost_CMT(int blkno);
void DFTL_Hit_Real_CMT(int blkno);

/***********************************************************************
 *             author:zhoujie       CPFTL  主函数逻辑实现
 ***********************************************************************/ 
void CPFTL_Scheme(int *pageno,int *req_size,int operation,int flash_flag);
/********************************************************
 *         author:zhoujie     CPFTL 封装的相关函数
 * *******************************************************/
void MLC_find_second_max();
void Hit_HCMT(int blkno,int operation);
void C_CMT_Is_Full();
void H_CMT_Is_Full();
void load_entry_into_C_CMT(int blkno,int operation);
void load_CCMT_or_SCMT_to_HCMT(int blkno,int operation);
void CPFTL_pre_load_entry_into_SCMT(int *pageno,int *req_size,int operation);

/***********************************************************************
 *                    debug function
***********************************************************************/
int CheckArrNum(int * arr,int max_num,int curr_num);
int  MLC_CheckArrStatus(int *arr,int max_num,int flag);
int MLC_find_second_min();
/***********************************************************************
  Cache
 ***********************************************************************/
int cache_min = -1;
int cache_max = 0;

// vaild_value >0
int CheckArrNum(int * arr,int max_num,int curr_num)
{
  int flag=0;
  int cnt=0,i;
  for(i=0;i< max_num;i++){
    if(arr[i]>0){
      cnt++;
    }
  }

  if(curr_num!=cnt){
    return cnt;
  }

  return flag;
}

int MLC_CheckArrStatus(int *arr,int max_num,int flag)
{
	int i,j,pos=-1;
	for(i=0;i<max_num;i++){
		if(arr[i]>0){
			if(MLC_opagemap[arr[i]].map_status!=flag){
				printf("LPN:%d MLC_opagemap[arr[%d]].map_status != flag :%d\n",arr[i],i,flag);
				return -1;

			}

		}
	}
/*
	// fu za du guo gao
	for(j=0;j<MLC_opagemap_num;j++){
		if(MLC_opagemap[j].map_status==flag){
			pos=search_table(arr,max_num,j);
			if(pos==-1){
				printf("MLC_opagemap[%d].map_status is flag:%d,but not in arr\n",j,flag);
				return -2;
			}
		}
	}
*/
	
	
	return 0;
}



// Interface between disksim & fsim 

void reset_flash_stat()
{
  flash_read_num =0;
  flash_write_num  =0;
  flash_gc_read_num = 0;
  flash_gc_write_num = 0; 
  flash_erase_num =0;
  flash_oob_read_num  =0;
  flash_oob_write_num  =0; 
}
void reset_SLC_flash_stat()
{
  SLC_flash_read_num  =0;
  SLC_flash_write_num  =0;
  SLC_flash_gc_read_num  = 0;
  SLC_flash_gc_write_num  = 0; 
  SLC_flash_erase_num  =0;
  SLC_flash_oob_read_num  =0;
  SLC_flash_oob_write_num  =0; 
}
void reset_MLC_flash_stat()
{
  MLC_flash_read_num =0;
  MLC_flash_write_num =0;
  MLC_flash_gc_read_num = 0;
  MLC_flash_gc_write_num = 0; 
  MLC_flash_erase_num =0;
  MLC_flash_oob_read_num =0;
  MLC_flash_oob_write_num =0; 
}
FILE *fp_flash_stat;
FILE *fp_gc;
FILE *fp_gc_timeseries;
double gc_di =0 ,gc_ti=0;


double calculate_delay_flash()
{
  double delay;
  double read_delay, write_delay;
  double erase_delay;
  double gc_read_delay, gc_write_delay;
  double oob_write_delay, oob_read_delay;

  oob_read_delay  = (double)OOB_READ_DELAY  * flash_oob_read_num;
  oob_write_delay = (double)OOB_WRITE_DELAY * flash_oob_write_num;

  read_delay     = (double)READ_DELAY  * flash_read_num; 
  write_delay    = (double)WRITE_DELAY * flash_write_num; 
  erase_delay    = (double)ERASE_DELAY * flash_erase_num; 

  gc_read_delay  = (double)GC_READ_DELAY  * flash_gc_read_num; 
  gc_write_delay = (double)GC_WRITE_DELAY * flash_gc_write_num; 


  delay = read_delay + write_delay + erase_delay + gc_read_delay + gc_write_delay + 
    oob_read_delay + oob_write_delay;

  if( flash_gc_read_num > 0 || flash_gc_write_num > 0 || flash_erase_num > 0 ) {
    gc_ti += delay;
  }
  else {
    gc_di += delay;
  }

  if(warm_done == 1){
    fprintf(fp_gc_timeseries, "%d\t%d\t%d\t%d\t%d\t%d\n", 
      req_count_num, merge_switch_num - old_merge_switch_num, 
      merge_partial_num - old_merge_partial_num, 
      merge_full_num - old_merge_full_num, 
      flash_gc_read_num,
      flash_erase_num);

    old_merge_switch_num = merge_switch_num;
    old_merge_partial_num = merge_partial_num;
    old_merge_full_num = merge_full_num;
    req_count_num++;
  }

  reset_flash_stat();

  return delay;
}
double calculate_delay_SLC_flash()
{
  double delay;
  double read_delay, write_delay;
  double erase_delay;
  double gc_read_delay, gc_write_delay;
  double oob_write_delay, oob_read_delay;

  oob_read_delay  = (double)SLC_OOB_READ_DELAY  * SLC_flash_oob_read_num;
  oob_write_delay = (double)SLC_OOB_WRITE_DELAY * SLC_flash_oob_write_num;

  read_delay     = (double)SLC_READ_DELAY  * SLC_flash_read_num; 
  write_delay    = (double)SLC_WRITE_DELAY * SLC_flash_write_num; 
  erase_delay    = (double)SLC_ERASE_DELAY * SLC_flash_erase_num; 

  gc_read_delay  = (double)SLC_GC_READ_DELAY  * SLC_flash_gc_read_num; 
  gc_write_delay = (double)SLC_GC_WRITE_DELAY * SLC_flash_gc_write_num; 


  delay = read_delay + write_delay + erase_delay + gc_read_delay + gc_write_delay + 
    oob_read_delay + oob_write_delay;

  if( SLC_flash_gc_read_num > 0 || SLC_flash_gc_write_num > 0 || SLC_flash_erase_num > 0 ) {
    gc_ti += delay;
  }
  else {
    gc_di += delay;
  }

  if(warm_done == 1){
    fprintf(fp_gc_timeseries, "%d\t%d\t%d\t%d\t%d\t%d\n", 
      req_count_num, merge_switch_num - old_merge_switch_num, 
      merge_partial_num - old_merge_partial_num, 
      merge_full_num - old_merge_full_num, 
      flash_gc_read_num,
      flash_erase_num);

    old_merge_switch_num = merge_switch_num;
    old_merge_partial_num = merge_partial_num;
    old_merge_full_num = merge_full_num;
    req_count_num++;
  }

  reset_SLC_flash_stat();

  return delay;
}
double calculate_delay_MLC_flash()
{
  double delay;
  double read_delay, write_delay;
  double erase_delay;
  double gc_read_delay, gc_write_delay;
  double oob_write_delay, oob_read_delay;

  oob_read_delay  = (double)MLC_OOB_READ_DELAY  * MLC_flash_oob_read_num;
  oob_write_delay = (double)MLC_OOB_WRITE_DELAY * MLC_flash_oob_write_num;

  read_delay     = (double)MLC_READ_DELAY  * MLC_flash_read_num; 
  write_delay    = (double)MLC_WRITE_DELAY * MLC_flash_write_num; 
  erase_delay    = (double)MLC_ERASE_DELAY * MLC_flash_erase_num; 

  gc_read_delay  = (double)MLC_GC_READ_DELAY  * MLC_flash_gc_read_num; 
  gc_write_delay = (double)MLC_GC_WRITE_DELAY * MLC_flash_gc_write_num; 


  delay = read_delay + write_delay + erase_delay + gc_read_delay + gc_write_delay + 
    oob_read_delay + oob_write_delay;

  if( MLC_flash_gc_read_num > 0 || MLC_flash_gc_write_num > 0 || MLC_flash_erase_num > 0 ) {
    gc_ti += delay;
  }
  else {
    gc_di += delay;
  }

  if(warm_done == 1){
    fprintf(fp_gc_timeseries, "%d\t%d\t%d\t%d\t%d\t%d\n", 
      req_count_num, merge_switch_num - old_merge_switch_num, 
      merge_partial_num - old_merge_partial_num, 
      merge_full_num - old_merge_full_num, 
      flash_gc_read_num,
      flash_erase_num);

    old_merge_switch_num = merge_switch_num;
    old_merge_partial_num = merge_partial_num;
    old_merge_full_num = merge_full_num;
    req_count_num++;
  }

  reset_MLC_flash_stat();

  return delay;
}

/***********************************************************************
  Initialize Flash Drive 
  ***********************************************************************/

void initFlash()
{
  blk_t total_blk_num;
  blk_t total_SLC_blk_num;
  blk_t total_MLC_blk_num;
  blk_t total_util_blk_num;
  blk_t total_extr_blk_num;
  blk_t total_SLC_util_blk_num;
  blk_t total_SLC_extr_blk_num;
  blk_t total_MLC_util_blk_num;
  blk_t total_MLC_extr_blk_num;

  // total number of sectors    
  // total_until_sect_num 和total_extra_blk_num都是宏定义在disksim_global.h中
  //   #define total_SLC_util_sect_num  1048576//1048576 --->(2k) 4096个块  512MB
  // #define total_SLC_extra_sect_num  32768//32768 -->(2k)  128个空闲块 16MB
  // 所以在配置文件应该配置为2G，保证MLC有1.5GB左右的剩余地址
  total_util_sect_num  = flash_numblocks;
  total_extra_sect_num = flash_extrblocks;
  total_MLC_util_sect_num  = total_util_sect_num-total_SLC_util_sect_num;
  total_MLC_extra_sect_num = total_extra_sect_num-total_SLC_extra_sect_num;
  total_sect_num = total_util_sect_num + total_extra_sect_num;
  total_SLC_sect_num=total_SLC_util_sect_num+total_SLC_extra_sect_num;
  total_MLC_sect_num=total_MLC_util_sect_num+total_MLC_extra_sect_num; 

  // total number of blocks 
  total_SLC_blk_num  = total_SLC_sect_num / S_SECT_NUM_PER_BLK;     
  total_MLC_blk_num  = total_MLC_sect_num / M_SECT_NUM_PER_BLK;
  total_blk_num =total_SLC_blk_num + total_MLC_blk_num; 

  total_SLC_util_blk_num = total_SLC_util_sect_num / S_SECT_NUM_PER_BLK;    
  total_MLC_util_blk_num = total_MLC_util_sect_num / M_SECT_NUM_PER_BLK;
  total_SLC_extr_blk_num = total_SLC_extra_sect_num / S_SECT_NUM_PER_BLK;
  total_MLC_extr_blk_num = total_MLC_extra_sect_num / M_SECT_NUM_PER_BLK;
  total_util_blk_num = total_SLC_util_blk_num + total_MLC_util_blk_num;
  global_total_blk_num = total_util_blk_num;

  total_extr_blk_num = total_blk_num - total_util_blk_num;        // total extra block number

  ASSERT(total_extr_blk_num != 0);

  if (nand_init(total_blk_num,total_SLC_blk_num,total_MLC_blk_num, 3) < 0) {
    EXIT(-4); 
  }

  switch(ftl_type){

    // pagemap
    case 1: ftl_op = pm_setup(); break;
    // blockmap
    //case 2: ftl_op = bm_setup(); break;
    // o-pagemap 
    case 3: ftl_op = opm_setup(); break;
    // fast
    case 4: ftl_op = lm_setup(); break;

    default: break;
  }

  ftl_op->init(total_SLC_util_blk_num,total_MLC_util_blk_num, total_extr_blk_num);

  nand_stat_reset();
}

void printWearout()
{
  int i;
  FILE *fp = fopen("wearout", "w");
  
  for(i = 0; i<nand_blk_num; i++)
  {
    fprintf(fp, "%d %d\n", i, nand_blk[i].state.ec); 
  }

  fclose(fp);
}


void endFlash()
{
  nand_stat_print(outputfile);
  ftl_op->end;
  nand_end();
}  

/***********************************************************************
  Send request (lsn, sector_cnt, operation flag)
  ***********************************************************************/


void send_flash_request(int start_blk_no, int block_cnt, int operation, int mapdir_flag,int flash_flag)
{
	int size;
	//size_t (*op_func)(sect_t lsn, size_t size);
	size_t (*op_func)(sect_t lsn, size_t size, int mapdir_flag);

        if((start_blk_no + block_cnt) >= total_util_sect_num){
          printf("start_blk_no: %d, block_cnt: %d, total_util_sect_num: %d\n", 
              start_blk_no, block_cnt, total_util_sect_num);
          exit(0);
        }
        if(flash_flag==0){
	   switch(operation){

	   //write
	   case 0:

		op_func = ftl_op->Sopm_write;
		while (block_cnt> 0) {
			size = op_func(start_blk_no, block_cnt, mapdir_flag);
			start_blk_no += size;
			block_cnt-=size;
		}
		break;
	  //read
	  case 1:


		op_func = ftl_op->Sopm_read;
		while (block_cnt> 0) {
			size = op_func(start_blk_no, block_cnt, mapdir_flag);
			start_blk_no += size;
			block_cnt-=size;
		}
		break;

	  default: 
		break;
	}
      }else{
         switch(operation){

	   //write
	   case 0:

		op_func = ftl_op->Mopm_write;
		while (block_cnt> 0) {
			size = op_func(start_blk_no, block_cnt, mapdir_flag);
			start_blk_no += size;
			block_cnt-=size;
		}
		break;
	  //read
	  case 1:


		op_func = ftl_op->Mopm_read;
		while (block_cnt> 0) {
			size = op_func(start_blk_no, block_cnt, mapdir_flag);
			start_blk_no += size;
			block_cnt-=size;
		}
		break;

	  default: 
		break;
	}
      }
}

void MLC_find_real_max()
{
  int i; 

  for(i=0;i < MAP_REAL_MAX_ENTRIES; i++) {
      if(MLC_opagemap[real_arr[i]].map_age > MLC_opagemap[real_max].map_age) {
          real_max = real_arr[i];
      }
  }
}

int MLC_find_real_min()
{
  
  int i,index; 
  int temp = 99999999;

  for(i=0; i < MAP_REAL_MAX_ENTRIES; i++) {
    if(real_arr[i]>0){
        if(MLC_opagemap[real_arr[i]].map_age <= temp) {
            real_min = real_arr[i];
            temp = MLC_opagemap[real_arr[i]].map_age;
            index = i;
        }
    }

  }
  return real_min;    
}


int MLC_find_ghost_min()
{
  int i,index; 
  int temp = 99999999;
  for(i=0; i < MAP_GHOST_MAX_ENTRIES; i++) {
    if(ghost_arr[i]>0){
        if(MLC_opagemap[ghost_arr[i]].map_age <= temp) {
            ghost_min = ghost_arr[i];
            temp = MLC_opagemap[ghost_arr[i]].map_age;
            index = i;
        }
    }

  }
  return ghost_min; 
}


void init_arr()
{

  int i;
  for( i = 0; i < MAP_REAL_MAX_ENTRIES; i++) {
      real_arr[i] = 0;
  }
  for( i= 0; i < MAP_SEQ_MAX_ENTRIES; i++) {
	  seq_arr[i] = 0;
  }
  for( i = 0; i < MAP_SECOND_MAX_ENTRIES; i++){
	  second_arr[i] = 0;
  }
  for( i = 0; i < CACHE_MAX_ENTRIES; i++) {
      cache_arr[i] = 0;

  MAP_REAL_NUM_ENTRIES=0;
  MAP_SECOND_NUM_ENTRIES=0;
  MAP_SEQ_NUM_ENTRIES=0;

  }
/*
  //设置翻译页，一个翻译页最多可以存储512个映射项
   block = (struct lru *)malloc(blocksize*sizeof(struct lru));//(struct lru *)强制类型转换
    for( i = 0; i < init_size; i++) 
    {//duchenjie: for CACHE_BLOCK
	  block[i].blknum = -1;//缓存初始块中所有的块不存储任何信息
	  block[i].age = 0;
    }*/
}

int search_table(int *arr, int size, int val) //等价于 int search_table(int arr[], int size, int val) ，形参数组名作为指针变量来处理
{
    int i;
    for(i =0 ; i < size; i++) {
       if(arr[i] == val) 
      {  //if ( *(arr + i) == val )
            return i;
        }
    }

	/* debug
    printf("shouldnt come here for search_table()=%d,%d",val,size);
    for( i = 0; i < size; i++) {
      if(arr[i] != 0) {
        printf("arr[%d]=%d ",i,arr[i]);
      }
    }
    exit(1);
	*/
    return -1;
}


int find_free_pos( int *arr, int size)
{
    int i;
    for(i = 0 ; i < size; i++) {
        if(arr[i] == 0) {   //可以改成 if ( *(arr + i ) == 0),  arr[i]和 *(arr+i) 是两个等价
            return i;
        }
    } 
	//   printf("shouldn't come here for find_free_pos()\n");
	//	exit(1);
    return -1;
}

void find_min_cache()
{
  int i; 
  int temp = 999999;

  for(i=0; i < CACHE_MAX_ENTRIES ;i++) {
      if(MLC_opagemap[cache_arr[i]].cache_age <= temp ) {
          cache_min = cache_arr[i];
          temp = MLC_opagemap[cache_arr[i]].cache_age;
      }
  }
}

void find_MC_entries(int *arr, int size)//find_MC_entries(second_arr,MAP_SECOND_MAX_ENTRIES);
{
	int i,j,k;
	int maxCount=1;
	int temp=99999999;
	int *arr1=(int *)malloc(sizeof(int)*size);
	for(k=0;k < size;k++)
		*(arr1+k)=(arr[k]-MLC_page_num_for_2nd_map_table)/MLC_MAP_ENTRIES_PER_PAGE;
	for(i=size-1;i>=0;i--)
	{
		if(arr1[i]!=temp)
		{
			for(j=i-1;j>=0;j--)
			{
				if(arr1[j]==arr1[i])//属于同一个簇
				{
					maxCount++;
					arr1[j]=temp;
				}
			}
			if(maxCount>=MC)
			{
				MC=maxCount;
				maxentry=arr1[i];
			}
			maxCount=1;
		}
	}
	free(arr1);
//	return i;
}

//~处理之后所有的请求映射项都不在CMT中才预取 
int not_in_cache(unsigned int pageno)
{
	int flag = 0;
	unsigned int i;
	for(i=pageno;i<pageno+NUM_ENTRIES_PER_TIME;i++)
	{
		if((MLC_opagemap[i].map_status == 0)||(MLC_opagemap[i].map_status == MAP_INVALID))
			flag=1;
		else
		{
			flag=0;
			break;
		}
	}
	return flag;
}

int Is_blk_in_second(int number,int num)
{
	int i; int flag1;
	for(i = number; i < (number+num); i++)
  {
	   if(MLC_opagemap[i].map_status != MAP_SECOND)
	   {
        flag1= 1;
     }
     else
     {
        flag1=0;
        break;
     }
  }//只要块中有一个页在SL-CMT中，就返回0
   return flag1;//块中所有的页都不在缓存中，就返回1
}

int youkim_flag1=0;

double callFsim(unsigned int secno, int scount, int operation,int flash_flag)
{
  double delay; 
  int bcount;
  unsigned int blkno; // pageno for page based FTL
  int cnt,z; int min_real;
  

  int pos=-1,pos_real=-1,pos_ghost=-1,pos_2nd=-1;
  
  if(ftl_type == 1){ }

  if(ftl_type == 3) {
      
      MLC_page_num_for_2nd_map_table = (MLC_opagemap_num / MLC_MAP_ENTRIES_PER_PAGE);
      if(youkim_flag1 == 0 ) {
        youkim_flag1 = 1;
        init_arr();
      }

      if((MLC_opagemap_num % MLC_MAP_ENTRIES_PER_PAGE) != 0){
        MLC_page_num_for_2nd_map_table++;
      }
  }
  //扇区和页对齐,转化 
  SecnoToPageno(secno,scount,&blkno,&bcount,flash_flag);

  cnt = bcount;
  total_request_size = total_request_size + bcount;
  if (bcount>2)
  {
      big_request_entry++;
      big_request_count+= bcount; 
  }

  while(cnt > 0)
  {
      cnt--;
      itemcount++;
      switch(ftl_type){
        case 1:
              // page based FTL
              send_flash_request(blkno*4, 4, operation, 1,0); 
                blkno++;
              break;
        case 2:
              // blck based FTL
              send_flash_request(blkno*4, 4, operation, 1,0); 
                blkno++;
              break;
        case 4: 
              // FAST scheme 
              if(operation == 0){
                write_count++;
                }
                else {
                  read_count++;
                }
                send_flash_request(blkno*4, 4, operation, 1,0); //cache_min is a page for page baseed FTL
                blkno++;
              break;
        case 3:
		 // SDFTL scheme
		 //SDFTL_Scheme(&blkno,&cnt,operation,flash_flag);
		// DFTL scheme
		//DFTL_Scheme(&blkno,&cnt,operation,flash_flag);
		// CPFTL scheme
		      CPFTL_Scheme(&blkno,&cnt,operation,flash_flag);
              break;
        }//end-switch

  }//end-while


  // 计算对应的时延
  if(flash_flag==0){
     delay = calculate_delay_SLC_flash();
  }
  else{
     delay = calculate_delay_MLC_flash();
  }
  return delay;
}


/************************CallFsim预处理函数**************************/
void SecnoToPageno(int secno,int scount,int *blkno,int *bcount,int flash_flag)
{
		 switch(ftl_type){
			 case 1:
						// page based FTL 
						*blkno = secno / 4;
						*bcount = (secno + scount -1)/4 - (secno)/4 + 1;
						break;
			case 2:
						// block based FTL 
						*blkno = secno/4;
						*bcount = (secno + scount -1)/4 - (secno)/4 + 1;      
						break;
			case 3:
						 // o-pagemap scheme
						if(flash_flag==0){ 
						   *blkno = secno / 4;
						   *bcount = (secno + scount -1)/4 - (secno)/4 + 1;
              //  CFTL的SLC只是简单的循环循环队列在这没有做统计
						  //  SLC_write_page_count+=(*bcount);
						}
						else{
						   *blkno = secno / 8;
						   *blkno += MLC_page_num_for_2nd_map_table;
						   *bcount = (secno + scount -1)/8 - (secno)/8 + 1;
						}
						break;
			case 4:
						// FAST scheme
						*blkno = secno/4;
						*bcount = (secno + scount -1)/4 - (secno)/4 + 1;
						break;		
		 }
}

/***********************************************************************
 *                    DFTL  主函数逻辑实现
 ***********************************************************************/ 
void DFTL_init_arr()
{
  int i;
  for( i= 0; i< MAP_GHOST_MAX_ENTRIES;i++){
    ghost_arr[i] = 0;
  }

  for( i = 0; i < MAP_REAL_MAX_ENTRIES; i++) {
      real_arr[i] = 0;
  }

  MAP_REAL_NUM_ENTRIES=0;
  MAP_GHOST_NUM_ENTRIES=0;
}

void DFTL_Scheme(int *pageno,int *req_size,int operation,int flash_flag)
{
  int blkno=(*pageno),cnt=(*req_size);
  int real_min=-1,ghost_min=-1,pos=-1;

  if(flash_flag==0){
    // 处理SLC简单
    send_flash_request(blkno*4,4,operation,1,0);
    blkno++;
  }else{
      //处理MLC 
      if (itemcount<itemcount_threshold){
        //利用trace数进行判断 
          rqst_cnt++;
          if(operation==0){
              write_count++;//用于计算总的写请求数    
          }
          else
            read_count++;
          blkno++;
      }else{
        if (itemcount==itemcount_threshold){
          //重要的初始化在此初始化
          request_cnt = rqst_cnt;
          write_cnt = write_count;
          read_cnt = read_count;
          write_ratio = (write_cnt*1.0)/request_cnt;//写请求比例
          read_ratio = (read_cnt*1.0)/request_cnt;  //读请求比列 
          average_request_size = (total_request_size*1.0)/itemcount;//请求平均大小
          // test debug 100
          MAP_REAL_MAX_ENTRIES=4096;
          real_arr=(int *)malloc(sizeof(int)*MAP_REAL_MAX_ENTRIES);
          // test debug 100
          MAP_GHOST_MAX_ENTRIES=821;
          ghost_arr=(int *)malloc(sizeof(int)*MAP_GHOST_MAX_ENTRIES);
          DFTL_init_arr();                             
        }
        
        rqst_cnt++;
        // req_entry in  SRAM
        if((MLC_opagemap[blkno].map_status==MAP_REAL)||(MLC_opagemap[blkno].map_status==MAP_GHOST)){
          cache_hit++;
          MLC_opagemap[blkno].map_age++;
          if(MLC_opagemap[blkno].map_status==MAP_GHOST){
			  //debug test
			 // printf("Hit Ghost CMT ,cache_hit %d\n",cache_hit);

              DFTL_Hit_Ghost_CMT(blkno);
          }else if(MLC_opagemap[blkno].map_status==MAP_REAL){
			  // debug test
			  // printf("Hit Real CMT ,cache_hit %d\n",cache_hit);
              DFTL_Hit_Real_CMT(blkno);
          }else{
            // debug
            printf("forbidden/shouldnt happen real =%d , ghost =%d\n",MAP_REAL,MAP_GHOST);
          }

          //2. opagemap not in SRAM 
        }else{
          // REAL CMT is FULL
          DFTL_Real_CMT_Full();
          //  read entry into REAL
          flash_hit++;
          send_flash_request(((blkno-MLC_page_num_for_2nd_map_table)/MLC_MAP_ENTRIES_PER_PAGE)*8,8,1,2,1);
          translation_read_num++;
          MLC_opagemap[blkno].map_status=MAP_REAL;
          MLC_opagemap[blkno].map_age=MLC_opagemap[real_max].map_age+1;
          real_max=blkno;
          MAP_REAL_NUM_ENTRIES++;
          pos=find_free_pos(real_arr,MAP_REAL_MAX_ENTRIES);
          // debug
          if(pos == -1){
            printf("can not find free pos in real_arr for %d LPN",blkno);
            assert(0);
          }
          real_arr[pos]=0;
          real_arr[pos]=blkno;
        }

        // write data or read data to flash
        if(operation==0){
          write_count++;
          MLC_opagemap[blkno].update=1;
        }else{
          read_count++;
        }
        send_flash_request(blkno*8,8,operation,1,1);
        blkno++;
      } 
  }

  (*pageno)=blkno;
  (*req_size)=cnt;
}

/********************************************************
 *              DFTL 封装的相关函数
 * *******************************************************/
void DFTL_Ghost_CMT_Full()
{
  int ghost_min=-1,pos=-1;
  if((MAP_GHOST_MAX_ENTRIES-MAP_GHOST_NUM_ENTRIES)==0){
      // Ghost CMT is full
      //evict one entry from ghost cache to DRAM or Disk, delay = DRAM or disk write, 1 oob write for invalidation
      ghost_min=MLC_find_ghost_min();
      // evict++;
      if(MLC_opagemap[ghost_min].update ==1){
        // update_reqd++;
        MLC_opagemap[ghost_min].update=0;
        // read from 2nd mapping table then update it
        send_flash_request(((ghost_min-MLC_page_num_for_2nd_map_table)/MLC_MAP_ENTRIES_PER_PAGE)*8,8,1,2,1);
        translation_read_num++;
        // write into 2nd mapping table 
        send_flash_request(((ghost_min-MLC_page_num_for_2nd_map_table)/MLC_MAP_ENTRIES_PER_PAGE)*8,8,0,2,1);
        translation_write_num++;
      }
      MLC_opagemap[ghost_min].map_status==MAP_INVALID;
      MLC_opagemap[ghost_min].map_age=0;

      //evict one entry from ghost cache 
      MAP_GHOST_NUM_ENTRIES--;
      pos=search_table(ghost_arr,MAP_GHOST_MAX_ENTRIES,ghost_min);
	  if(pos==-1){
		printf("can not find ghost_min:%d  in ghost_arr\n",ghost_min);
		assert(0);
	  }
      ghost_arr[pos]=0;
    }
}

void DFTL_Real_CMT_Full()
{
  int real_min=-1,pos=-1;
  if((MAP_REAL_MAX_ENTRIES-MAP_REAL_NUM_ENTRIES)==0){
    // check Ghost is full?
    DFTL_Ghost_CMT_Full();
    //evict one entry from real cache to ghost cache 
    MAP_REAL_NUM_ENTRIES--;
    real_min=MLC_find_real_min();
    MLC_opagemap[real_min].map_status=MAP_GHOST;
    pos = search_table(real_arr,MAP_REAL_MAX_ENTRIES,real_min);
	if(pos==-1){
		printf("can not find real_min:%d  in real_arr\n",real_min);
		assert(0);
	}	  
    real_arr[pos]=0;
    pos=find_free_pos(ghost_arr,MAP_GHOST_MAX_ENTRIES);
    ghost_arr[pos]=real_min;
    MAP_GHOST_NUM_ENTRIES++;
  }
}

void DFTL_Hit_Ghost_CMT(int blkno)
{

  int real_min=-1;
  int pos_ghost=-1,pos_real=-1;
  real_min=MLC_find_real_min();
  // 注意交换状态和数组数据
  if(MLC_opagemap[real_min].map_age<=MLC_opagemap[blkno].map_age){
    // 两者相互交换状态
    MLC_opagemap[blkno].map_status=MAP_REAL;
    MLC_opagemap[real_min].map_status=MAP_GHOST;

    pos_ghost=search_table(ghost_arr,MAP_GHOST_MAX_ENTRIES,blkno);
	if(pos_ghost==-1){
		printf("can not find blkno:%d  in ghost_arr\n",blkno);
		assert(0);
	}	  
    ghost_arr[pos_ghost]=0;

    pos_real=search_table(real_arr,MAP_REAL_MAX_ENTRIES,real_min);
	if(pos_real==-1){
		printf("can not find real_min:%d  in real_arr\n",real_min);
		assert(0);
	}	  
    real_arr[pos_real]=0;

    real_arr[pos_real]=blkno;
    ghost_arr[pos_ghost]=real_min;

  }
  // test debug 
  if(CheckArrNum(ghost_arr,MAP_GHOST_MAX_ENTRIES,MAP_GHOST_NUM_ENTRIES)){
    printf("ghost arr curr_num is %d, count num is%d",CheckArrNum(ghost_arr,MAP_GHOST_MAX_ENTRIES,MAP_GHOST_NUM_ENTRIES),MAP_GHOST_NUM_ENTRIES);
    assert(0);
  }

  if(CheckArrNum(real_arr,MAP_REAL_MAX_ENTRIES,MAP_REAL_NUM_ENTRIES)){
    printf("real arr curr_num is %d, count num is%d",CheckArrNum(real_arr,MAP_REAL_MAX_ENTRIES,MAP_REAL_NUM_ENTRIES),MAP_REAL_NUM_ENTRIES);
    assert(0);
  }

}

void DFTL_Hit_Real_CMT(int blkno)
{
    if(real_max==-1){
      real_max = 0;
      MLC_find_real_max();
      printf("Never happend\n");
    }
    if(MLC_opagemap[real_max].map_age<=MLC_opagemap[blkno].map_age){
      real_max=blkno;
    }
}


/***********************************************************************
 *                        SDFTL 主函数逻辑
 ***********************************************************************/
 void SDFTL_Scheme(int *pageno,int *req_size,int operation,int flash_flag)
{
  	//代码封装过程中的中间变量
	int blkno=(*pageno),cnt=(*req_size);

  if(flash_flag==0){
      send_flash_request(blkno*4, 4, operation, 1,0); 
      blkno++;
    }
    else{

      if (itemcount<itemcount_threshold)
      {
        //利用trace数进行判断 
          rqst_cnt++;
          if(operation==0){
              write_count++;//用于计算总的写请求数    
          }
          else
            read_count++;
          blkno++;
      }
      else{

        if (itemcount==itemcount_threshold){
          request_cnt = rqst_cnt;
          write_cnt = write_count;
          read_cnt = read_count;
          write_ratio = (write_cnt*1.0)/request_cnt;//写请求比例
          read_ratio = (read_cnt*1.0)/request_cnt;  //读请求比列 
          
          average_request_size = (total_request_size*1.0)/itemcount;//请求平均大小

          MAP_REAL_MAX_ENTRIES=4096;
          real_arr=(int *)malloc(sizeof(int)*MAP_REAL_MAX_ENTRIES);
          //MAP_GHOST_MAX_ENTRIES=822;
          //ghost_arr=(int *)malloc(sizeof(int)*MAP_GHOST_MAX_ENTRIES);
          MAP_SEQ_MAX_ENTRIES=1536; 
          seq_arr=(int *)malloc(sizeof(int)*MAP_SEQ_MAX_ENTRIES); 
          MAP_SECOND_MAX_ENTRIES=2560; 
          second_arr=(int *)malloc(sizeof(int)*MAP_SECOND_MAX_ENTRIES); 
          init_arr();                             
        }
          rqst_cnt++;
          //duchenjie:no ghost
        if (MLC_opagemap[blkno].map_status == MAP_REAL){
            Hit_CMT_Entry(blkno,operation);
            blkno++;
        }
        //2.shzb:请求在连续缓存中
        else if((MLC_opagemap[blkno].map_status == MAP_SEQ)||(MLC_opagemap[blkno].map_status == MAP_SECOND))
        {
            //			cache_hit++;
            if(MLC_opagemap[blkno].map_status == MAP_SEQ){
                Hit_SCMT_Entry(blkno,operation);
            }
            else {  
                Hit_SL_CMT_Entry(blkno,operation);
            }
              blkno++;
        }
        //3.shzb:连续请求加入连续缓存中
        else if((cnt+1) >= THRESHOLD)
        {
            //shzb:THRESHOLD=2,表示大于或等于4KB的请求，当作连续请求来处理。
            pre_load_entry_into_SCMT(&blkno,&cnt,operation);
        }else{
            //4. opagemap not in SRAM  must think about if map table in SRAM is full
            req_Entry_Miss_SDFTL(blkno,operation);
            blkno++;
        }
      }

    }

  //注意变量换回赋值
  (*pageno)=blkno;
  (*req_size)=cnt;

}

/**********************************************************
 * 								SDFTL  执行内部封装的函数 CFTL没有region一说
 * ********************************************************/
// hit CMT-entry
void Hit_CMT_Entry(int blkno,int operation)
{
			cache_cmt_hit++;
			MLC_opagemap[blkno].map_age = MLC_opagemap[real_max].map_age + 1;//LRU
			real_max = blkno;

		  if(MLC_opagemap[real_max].map_age <= MLC_opagemap[blkno].map_age){
				real_max = blkno;
		  }  
		
		  if(operation==0){
				write_count++;
				MLC_opagemap[blkno].update = 1;
		  }
		  else
				read_count++;
				
		  send_flash_request(blkno*8, 8, operation, 1,1); 
}


//hit SL_CMT-entry
void Hit_SL_CMT_Entry(int blkno,int operation)
{
	cache_slcmt_hit++;
	  if(operation==0){
			write_count++;
			MLC_opagemap[blkno].update = 1;
	  }
	  else
		 read_count++;

	  send_flash_request(blkno*8, 8, operation, 1,1); 
}


//Hit_SCM_Entry
void Hit_SCMT_Entry(int blkno,int operation)
{
		int pos=-1;
		cache_scmt_hit++;
		MLC_opagemap[blkno].map_age++;
		if(MLC_opagemap[blkno].map_age >1){
						//把该映射项加载到real中,CMT is Full(首先查看是否满)
						CMT_Is_Full();
						//将映射关系从SCMT中加载到CMT中
						MLC_opagemap[blkno].map_status = MAP_REAL;
						MLC_opagemap[blkno].map_age = MLC_opagemap[real_max].map_age + 1;
						real_max = blkno;      
						pos = find_free_pos(real_arr,MAP_REAL_MAX_ENTRIES);
						real_arr[pos] = 0;
						real_arr[pos] = blkno;
						MAP_REAL_NUM_ENTRIES++;
			}

			//数据页写入读取更新，统计
			if(operation==0){
					write_count++;
					MLC_opagemap[blkno].update = 1;
			}
			else
					read_count++;
					
			send_flash_request(blkno*8, 8, operation, 1,1); 
}  

//CMT is Full
void CMT_Is_Full()
{
	int min_real,pos=-1,pos_2nd=-1;
	// 删除的映射项如果更新了话,导入到SLCMT中进一步延迟删除
	//如果没有更新,则直接删除
		if((MAP_REAL_MAX_ENTRIES - MAP_REAL_NUM_ENTRIES) == 0){  
				min_real = MLC_find_real_min();
				if(MLC_opagemap[min_real].update == 1)
				{
					//如果SLCMT满了,则聚簇选择最大的关联度的翻译页回写
						if((MAP_SECOND_MAX_ENTRIES-MAP_SECOND_NUM_ENTRIES) ==0){
								 MC=0;
								find_MC_entries(second_arr,MAP_SECOND_MAX_ENTRIES);
								send_flash_request(maxentry*8,8,1,2,1);
								translation_read_num++;
								send_flash_request(maxentry*8,8,0,2,1);
								translation_write_num++;
								//sencond_arr数组里面存的是lpn,将翻译页关联的映射项全部置为无效
								for(indexold = 0;indexold < MAP_SECOND_MAX_ENTRIES; indexold++){
									
										if(((second_arr[indexold]-MLC_page_num_for_2nd_map_table)/MLC_MAP_ENTRIES_PER_PAGE) == maxentry){
											MLC_opagemap[second_arr[indexold]].update = 0;
											MLC_opagemap[second_arr[indexold]].map_status = MAP_INVALID;
											MLC_opagemap[second_arr[indexold]].map_age = 0;
											second_arr[indexold]=0;
											MAP_SECOND_NUM_ENTRIES--;
										}
									}
								
						}
						//将CMT中更新的映射项剔除到SL-CMT中
						MLC_opagemap[min_real].map_status = MAP_SECOND;
						pos = search_table(real_arr,MAP_REAL_MAX_ENTRIES,min_real);
						if(pos==-1){
							printf("can not find min_real:%d  in real_arr\n",min_real);
							assert(0);
						}	  
						real_arr[pos]=0;
						MAP_REAL_NUM_ENTRIES--;
						pos_2nd = find_free_pos(second_arr,MAP_SECOND_MAX_ENTRIES);
						second_arr[pos_2nd]=0;
						second_arr[pos_2nd]=min_real;
						MAP_SECOND_NUM_ENTRIES++;
						//debug
						if(MAP_SECOND_NUM_ENTRIES > MAP_SECOND_MAX_ENTRIES){
							printf("The second cache is overflow!\n");
							assert(0);
						}
				}else{
					//没有更新的直接删除
							pos = search_table(real_arr,MAP_REAL_MAX_ENTRIES,min_real);
							if(pos==-1){
								printf("can not find min_real:%d  in real_arr\n",min_real);
								assert(0);
							}	  
							real_arr[pos]=0;
							MLC_opagemap[min_real].map_status = MAP_INVALID;
							MLC_opagemap[min_real].map_age = 0;
							MAP_REAL_NUM_ENTRIES--;
				}

	}
			
}


//pre load entry into SCMT
void pre_load_entry_into_SCMT(int *pageno,int *req_size,int operation)
{
	
		int pos=-1,min_real,pos_2nd=-1;
		int blkno=(*pageno),cnt=(*req_size);
		if(not_in_cache(blkno)){
			//~ blkno entry not in CMT need pre_load  
				MLC_opagemap[blkno].map_age++;
				if((MAP_SEQ_MAX_ENTRIES-MAP_SEQ_NUM_ENTRIES)==0)
				{
						for(indexofarr = 0;indexofarr < NUM_ENTRIES_PER_TIME;indexofarr++)
						{
							//SEQ的替换策略是把SEQ的最前面几个映射项剔除出去
							if((MLC_opagemap[seq_arr[indexofarr]].update == 1)&&(MLC_opagemap[seq_arr[indexofarr]].map_status == MAP_SEQ))
							{
								//update_reqd++;
								update_flag=1;
								MLC_opagemap[seq_arr[indexofarr]].update=0;
								MLC_opagemap[seq_arr[indexofarr]].map_status = MAP_INVALID;
								MLC_opagemap[seq_arr[indexofarr]].map_age = 0;
							}
							else if((MLC_opagemap[seq_arr[indexofarr]].update == 0)&&(MLC_opagemap[seq_arr[indexofarr]].map_status ==MAP_SEQ))
							{
								MLC_opagemap[seq_arr[indexofarr]].map_status = MAP_INVALID;
								MLC_opagemap[seq_arr[indexofarr]].map_age = 0;
							}
						}
						if(update_flag == 1)
						{
								send_flash_request(((seq_arr[0]-MLC_page_num_for_2nd_map_table)/MLC_MAP_ENTRIES_PER_PAGE)*8,8,1,2,1);
								translation_read_num++;
								send_flash_request(((seq_arr[0]-MLC_page_num_for_2nd_map_table)/MLC_MAP_ENTRIES_PER_PAGE)*8,8,0,2,1);
								translation_write_num++;
								update_flag=0;
						}
						for(indexofarr = 0;indexofarr <= MAP_SEQ_MAX_ENTRIES-1-NUM_ENTRIES_PER_TIME; indexofarr++)
								seq_arr[indexofarr] = seq_arr[indexofarr+NUM_ENTRIES_PER_TIME];
						MAP_SEQ_NUM_ENTRIES-=NUM_ENTRIES_PER_TIME;
				}
				flash_hit++;
				send_flash_request(((blkno-MLC_page_num_for_2nd_map_table)/MLC_MAP_ENTRIES_PER_PAGE)*8, 8, 1, 2,1);
				translation_read_num++;
				for(indexofseq=0; indexofseq < NUM_ENTRIES_PER_TIME;indexofseq++)//NUM_ENTRIES_PER_TIME在这里表示一次加载4个映射表信息
				{
					MLC_opagemap[blkno+indexofseq].map_status=MAP_SEQ;
					seq_arr[MAP_SEQ_NUM_ENTRIES] = (blkno+indexofseq);//加载到SEQ的映射项是放在SEQ尾部
					MAP_SEQ_NUM_ENTRIES++;
				}
				if(MAP_SEQ_NUM_ENTRIES > MAP_SEQ_MAX_ENTRIES)
				{
					printf("The sequential cache is overflow!\n");
					exit(0);
				}
				if(operation==0)
				{
						write_count++;
						MLC_opagemap[blkno].update=1;
				}
				else
						read_count++;
				send_flash_request(blkno*8,8,operation,1,1);

				blkno++;
				sequential_count = 0;
				//之后连续的请求因为映射加载完成直接读取写入操作
				for(;(cnt>0)&&(sequential_count<NUM_ENTRIES_PER_TIME-1);cnt--)
				{
					MLC_opagemap[blkno].map_age++;
					cache_scmt_hit++;
					if(operation==0)
					{
						write_count++;
						MLC_opagemap[blkno].update=1;
					}
					else
						read_count++;
					send_flash_request(blkno*8,8,operation,1,1);
					blkno++;
					rqst_cnt++;
					sequential_count++;
				}
				
				//zhoujie
				*req_size=cnt;
				*pageno=blkno;

		}
	   
	   else{
					if((MAP_REAL_MAX_ENTRIES - MAP_REAL_NUM_ENTRIES) == 0)
					{
							min_real = MLC_find_real_min();
							if(MLC_opagemap[min_real].update == 1)
							{
									if((MAP_SECOND_MAX_ENTRIES-MAP_SECOND_NUM_ENTRIES) ==0)
									{
											MC=0;
											find_MC_entries(second_arr,MAP_SECOND_MAX_ENTRIES);
											send_flash_request(maxentry*8,8,1,2,1);
											translation_read_num++;
											send_flash_request(maxentry*8,8,0,2,1);
											translation_write_num++;
											for(indexold = 0;indexold < MAP_SECOND_MAX_ENTRIES; indexold++)
											{
													if(((second_arr[indexold]-MLC_page_num_for_2nd_map_table)/MLC_MAP_ENTRIES_PER_PAGE) == maxentry)
													{
															MLC_opagemap[second_arr[indexold]].update = 0;
															MLC_opagemap[second_arr[indexold]].map_status = MAP_INVALID;
															MLC_opagemap[second_arr[indexold]].map_age = 0;
															second_arr[indexold]=0;
															MAP_SECOND_NUM_ENTRIES--;
													}
											}
									}
									MLC_opagemap[min_real].map_status = MAP_SECOND;
									pos = search_table(real_arr,MAP_REAL_MAX_ENTRIES,min_real);
									if(pos==-1){
										printf("can not find min_real:%d  in real_arr\n",min_real);
										assert(0);
									}	  
									real_arr[pos]=0;
									MAP_REAL_NUM_ENTRIES--;
									pos_2nd = find_free_pos(second_arr,MAP_SECOND_MAX_ENTRIES);
									second_arr[pos_2nd]=0;
									second_arr[pos_2nd]=min_real;
									MAP_SECOND_NUM_ENTRIES++;
									if(MAP_SECOND_NUM_ENTRIES > MAP_SECOND_MAX_ENTRIES)
									{
											printf("The second cache is overflow!\n");
											exit(0);
									}

							}
							else
							{
								pos = search_table(real_arr,MAP_REAL_MAX_ENTRIES,min_real);
								if(pos==-1){
									printf("can not find min_real:%d  in real_arr\n",min_real);
									assert(0);
								}	  
								real_arr[pos]=0;
								MLC_opagemap[min_real].map_status = MAP_INVALID;
								MLC_opagemap[min_real].map_age = 0;
								MAP_REAL_NUM_ENTRIES--;
							}

					} 
					
					flash_hit++;
					send_flash_request(((blkno-MLC_page_num_for_2nd_map_table)/MLC_MAP_ENTRIES_PER_PAGE)*8, 8, 1, 2,1);   // read from 2nd mapping table
					translation_read_num++;
					MLC_opagemap[blkno].map_status = MAP_REAL;

					MLC_opagemap[blkno].map_age = MLC_opagemap[real_max].map_age + 1;
					real_max = blkno;

					pos = find_free_pos(real_arr,MAP_REAL_MAX_ENTRIES);//因为real_arr已经被定义成指针变量，调用find_free_pos函数时前面不需要加*
					real_arr[pos] = -1;
					real_arr[pos] = blkno;
					MAP_REAL_NUM_ENTRIES++;
					if(operation==0){
						write_count++;
						MLC_opagemap[blkno].update = 1;
					}
					else
						read_count++;

					send_flash_request(blkno*8, 8, operation, 1,1); 
					blkno++;
					
					//zhoujie
					*pageno=blkno;
					*req_size=cnt;
					
		}
															   
															   
															   
															   
}

//req entry not in cache,and req not seq_req
void req_Entry_Miss_SDFTL(int blkno,int operation)
{
		int min_real;
		int pos_2nd=-1,pos=-1;
		min_real = MLC_find_real_min();
  //    第一次加载的数据页映射项新加载到CMT中,所以需要检测对应的CMT是否满
		CMT_Is_Full();
	
		flash_hit++;
		send_flash_request(((blkno-MLC_page_num_for_2nd_map_table)/MLC_MAP_ENTRIES_PER_PAGE)*8, 8, 1, 2,1);   // read from 2nd mapping table
		translation_read_num++;
		MLC_opagemap[blkno].map_status = MAP_REAL;

		MLC_opagemap[blkno].map_age = MLC_opagemap[real_max].map_age + 1;
		real_max = blkno;

		pos = find_free_pos(real_arr,MAP_REAL_MAX_ENTRIES);//因为real_arr已经被定义成指针变量，调用find_free_pos函数时前面不需要加*
		real_arr[pos] = 0;
		real_arr[pos] = blkno;
		MAP_REAL_NUM_ENTRIES++;
	  if(operation==0){
			write_count++;
			MLC_opagemap[blkno].update = 1;
		}
	  else
			read_count++;

		send_flash_request(blkno*8, 8, operation, 1,1);
	
}



/****************************************************
 *              CPFTL  主函数
 * **************************************************/
void CPFTL_Scheme(int *pageno,int *req_size,int operation,int flash_flag)
{
  int blkno=(*pageno),cnt=(*req_size);
  int pos=-1,free_pos=-1;
  if(flash_flag==0){
      send_flash_request(blkno*4, 4, operation, 1,0); 
      blkno++;
    }
    else{

      if (itemcount<itemcount_threshold)
      {
        //利用trace数进行判断 
          rqst_cnt++;
          if(operation==0){
              write_count++;//用于计算总的写请求数    
          }
          else
            read_count++;
          blkno++;
      }
      else{

        if (itemcount==itemcount_threshold){
          request_cnt = rqst_cnt;
          write_cnt = write_count;
          read_cnt = read_count;
          write_ratio = (write_cnt*1.0)/request_cnt;//写请求比例
          read_ratio = (read_cnt*1.0)/request_cnt;  //读请求比列 
          
          average_request_size = (total_request_size*1.0)/itemcount;//请求平均大小

		  //test set 100
          MAP_REAL_MAX_ENTRIES=100;
          // real_arr 当做H-CMT使用
          real_arr=(int *)malloc(sizeof(int)*MAP_REAL_MAX_ENTRIES);
          // test_debug -->old 1536
          MAP_SEQ_MAX_ENTRIES=160;
          // seq_arr当做S-CMT使用 
          seq_arr=(int *)malloc(sizeof(int)*MAP_SEQ_MAX_ENTRIES); 
		  //test set 100
          MAP_SECOND_MAX_ENTRIES=100;
          // second_arr当做C-CMT使用 
          second_arr=(int *)malloc(sizeof(int)*MAP_SECOND_MAX_ENTRIES); 
          init_arr();                             
        }
          rqst_cnt++;
          // 此处开始SDFTL函数的逻辑
          if(MLC_opagemap[blkno].map_status==MAP_REAL){
            // 1. req in H-CMT
            Hit_HCMT(blkno,operation);
			//debug test
			if(MLC_CheckArrStatus(second_arr,MAP_SECOND_MAX_ENTRIES,MAP_SECOND)!=0){
				printf("second_arr status error int req in H_CMT");
				assert(0);
			}
            blkno++;
          }else if(MLC_opagemap[blkno].map_status==MAP_SECOND || MLC_opagemap[blkno].map_status==MAP_SEQ){
            // 2. req in C-CMT or S-CMT
            // load H-CMT is full

			//debug test
			if(MLC_opagemap[blkno].map_status==MAP_SECOND){
				if(search_table(second_arr,MAP_SECOND_MAX_ENTRIES,blkno)==-1){
					printf("before second arr is error\n");
					assert(0);
				}
			}
            
            MLC_opagemap[blkno].map_age=sys_time;
            sys_time++;
            H_CMT_Is_Full();


            //debug test
			if(MLC_opagemap[blkno].map_status==MAP_SECOND){
				if(search_table(second_arr,MAP_SECOND_MAX_ENTRIES,blkno)==-1){
					printf("after second arr is error\n");
					assert(0);
				}
			}
            
            load_CCMT_or_SCMT_to_HCMT(blkno,operation);
            blkno++;

          }
		  else if((cnt+1) >=10 ){
			//THRESHOLD=10
            // 3. THRESHOLD=2,表示大于或等于4KB的请求，当作连续请求来处理
            //内部对blkno和cnt做了更新
				// debug
				int last_cnt=cnt;
				int last_blkno=blkno;
				
				CPFTL_pre_load_entry_into_SCMT(&blkno,&cnt,operation);

				//debug test
				if(cnt>0){
					int i=0;
					for(i=0;i<=last_cnt;i++){
						last_blkno++;
						if(MLC_opagemap[last_blkno].map_status==MAP_SECOND){
							if(search_table(second_arr,MAP_SECOND_MAX_ENTRIES,last_blkno)==-1){
								printf("error happend after CPFTL_pre_load_entry_into_SCMT\n");
								printf("second_arr not include %d",last_blkno);
								assert(0);
							}
						}
					}
				}
				//debug test
				
          }
          else{
            //4. opagemap not in SRAM  must think about if map table in SRAM(C-CMT) is full
            C_CMT_Is_Full();
            // load entry into C_CMT
            load_entry_into_C_CMT(blkno,operation);

            //debug test
            if(MLC_opagemap[blkno].map_status==MAP_SECOND){
				if(search_table(second_arr,MAP_SECOND_MAX_ENTRIES,blkno)==-1){
					printf("load new-entry into second arr is failed\n");
					assert(0);
				}
			}

            blkno++;
          }
          //CMT中的各种情况处理完毕
      }
      //MLC中的映射关系处理完毕
	}



  // update return data
  (*pageno)=blkno;
  (*req_size)=cnt;
}

/****************************************************
 *              CPFTL  封装函数
 * **************************************************/
void MLC_find_second_max()
{
  int i; 

  for(i=0;i < MAP_SECOND_MAX_ENTRIES; i++) {
    if(second_arr[i]>0){
      if(MLC_opagemap[second_arr[i]].map_age > MLC_opagemap[second_max].map_age) {
          second_max = second_arr[i];
      }
    }
  }

}

void Hit_HCMT(int blkno,int operation)
{
	MLC_opagemap[blkno].map_status=MAP_REAL;
	MLC_opagemap[blkno].map_age=sys_time;
	sys_time++;
	  // write or read data page 
 	  if(operation==0){
			write_count++;
			MLC_opagemap[blkno].update = 1;
		}
	  else
			read_count++;

		send_flash_request(blkno*8, 8, operation, 1,1); 
}

void H_CMT_Is_Full()
{
  int min_real,pos=-1,pos_2nd=-1;
  // 查看H_CMT是否满了
  if((MAP_REAL_MAX_ENTRIES - MAP_REAL_NUM_ENTRIES) == 0){  
				min_real = MLC_find_real_min();
				if(MLC_opagemap[min_real].update == 1){
						C_CMT_Is_Full();
						//将H-CMT中更新的映射项剔除到C-CMT中
						MLC_opagemap[min_real].map_status = MAP_SECOND;
						pos = search_table(real_arr,MAP_REAL_MAX_ENTRIES,min_real);
						real_arr[pos]=0;
						MAP_REAL_NUM_ENTRIES--;
						pos_2nd = find_free_pos(second_arr,MAP_SECOND_MAX_ENTRIES);
						second_arr[pos_2nd]=0;
						second_arr[pos_2nd]=min_real;
						MAP_SECOND_NUM_ENTRIES++;
						//debug
						if(MAP_SECOND_NUM_ENTRIES > MAP_SECOND_MAX_ENTRIES){
							printf("The second cache is overflow!\n");
							assert(0);
						}

						//debug test
						if(MLC_opagemap[min_real].map_status==MAP_SECOND && search_table(second_arr,MAP_SECOND_MAX_ENTRIES,min_real)==-1){
							printf("not reset min_real into second_arr\n",min_real);
							assert(0);
						}
        }else{
          // 没有更新直接剔除到flash
          		pos = search_table(real_arr,MAP_REAL_MAX_ENTRIES,min_real);
							real_arr[pos]=0;
							MLC_opagemap[min_real].map_status = MAP_INVALID;
							MLC_opagemap[min_real].map_age = 0;
							MAP_REAL_NUM_ENTRIES--;
        }
  }

    
}


void CPFTL_pre_load_entry_into_SCMT(int *pageno,int *req_size,int operation)
{
	int blkno=(*pageno),cnt=(*req_size);
	int pos=-1,free_pos=-1;
	
	if(not_in_cache(blkno)){
		//~满足之后连续的pre_num个数的映射项不在CMT中才预取 
		if((MAP_SEQ_MAX_ENTRIES-MAP_SEQ_NUM_ENTRIES)==0){
			//~如果seq_arr满了
			 for(indexofarr = 0;indexofarr < NUM_ENTRIES_PER_TIME;indexofarr++){
					//SEQ的替换策略是把SEQ的最前面几个映射项剔除出去，查看是否存在更新页，存在一个更新页，整个翻译页都得重新更细，update_flag置位
					if((MLC_opagemap[seq_arr[indexofarr]].update == 1)&&(MLC_opagemap[seq_arr[indexofarr]].map_status == MAP_SEQ))
					{
						//update_reqd++;
						update_flag=1;
						MLC_opagemap[seq_arr[indexofarr]].update=0;
						MLC_opagemap[seq_arr[indexofarr]].map_status = MAP_INVALID;
						MLC_opagemap[seq_arr[indexofarr]].map_age = 0;
					}
					else if((MLC_opagemap[seq_arr[indexofarr]].update == 0)&&(MLC_opagemap[seq_arr[indexofarr]].map_status ==MAP_SEQ))
					{
						MLC_opagemap[seq_arr[indexofarr]].map_status = MAP_INVALID;
						MLC_opagemap[seq_arr[indexofarr]].map_age = 0;
					}
				}
				if(update_flag == 1)
				{
					//~这里的NUM_ENTRIES_PER_TIME必须远小于一个页
						send_flash_request(((seq_arr[0]-MLC_page_num_for_2nd_map_table)/MLC_MAP_ENTRIES_PER_PAGE)*8,8,1,2,1);
						translation_read_num++;
						send_flash_request(((seq_arr[0]-MLC_page_num_for_2nd_map_table)/MLC_MAP_ENTRIES_PER_PAGE)*8,8,0,2,1);
						translation_write_num++;
						update_flag=0;
				}
				//~ 根据FIFO移动数组数据覆盖 
				for(indexofarr = 0;indexofarr <= MAP_SEQ_MAX_ENTRIES-1-NUM_ENTRIES_PER_TIME; indexofarr++)
						seq_arr[indexofarr] = seq_arr[indexofarr+NUM_ENTRIES_PER_TIME];
				MAP_SEQ_NUM_ENTRIES-=NUM_ENTRIES_PER_TIME;
			}
			//~ 剔除依据FIFO完毕 ，开始预取
			flash_hit++;
			send_flash_request(((blkno-MLC_page_num_for_2nd_map_table)/MLC_MAP_ENTRIES_PER_PAGE)*8, 8, 1, 2,1);
			translation_read_num++;
			for(indexofseq=0; indexofseq < NUM_ENTRIES_PER_TIME;indexofseq++)//NUM_ENTRIES_PER_TIME在这里表示一次加载4个映射表信息
			{
				MLC_opagemap[blkno+indexofseq].map_status=MAP_SEQ;
				MLC_opagemap[blkno+indexofseq].map_age=sys_time;
				sys_time++;
				//~对应的seq命中转移数据页应该不能删除原来的数据 
				seq_arr[MAP_SEQ_NUM_ENTRIES] = (blkno+indexofseq);//加载到SEQ的映射项是放在SEQ尾部
				MAP_SEQ_NUM_ENTRIES++;
			}
			if(MAP_SEQ_NUM_ENTRIES > MAP_SEQ_MAX_ENTRIES)
			{
				printf("The sequential cache is overflow!\n");
				exit(0);
			}
			//~  对数据页读取写入操作更新 
			if(operation==0)
				{
						write_count++;
						MLC_opagemap[blkno].update=1;
				}
				else
						read_count++;
				send_flash_request(blkno*8,8,operation,1,1);

				blkno++;
				sequential_count = 0;
				//之后连续的请求因为映射加载完成直接读取写入操作
				for(;(cnt>0)&&(sequential_count<NUM_ENTRIES_PER_TIME-1);cnt--)
				{
					MLC_opagemap[blkno].map_age++;
					cache_scmt_hit++;
					if(operation==0)
					{
						write_count++;
						MLC_opagemap[blkno].update=1;
					}
					else
						read_count++;
					send_flash_request(blkno*8,8,operation,1,1);
					blkno++;
					rqst_cnt++;
					sequential_count++;
				}
				
				//zhoujie
				*req_size=cnt;
				*pageno=blkno;
			
	}else{
		//~反之只是将其一个映射项加载到CCMT中 (根据前面的删选，只能是不存在CMT的映射项)
		C_CMT_Is_Full();
		load_entry_into_C_CMT(blkno,operation);
		blkno++;
		//zhoujie
		*req_size=cnt;
		*pageno=blkno;	
	}
	//连续加载处理完成
}


void load_entry_into_C_CMT(int blkno,int operation)
{
    int free_pos=-1;
    flash_hit++;
    // read MVPN page 
	 send_flash_request(((blkno-MLC_page_num_for_2nd_map_table)/MLC_MAP_ENTRIES_PER_PAGE)*8, 8, 1, 2,1);   // read from 2nd mapping table
	translation_read_num++;
	MLC_opagemap[blkno].map_status = MAP_SECOND;

  MLC_opagemap[blkno].map_age=sys_time;
  sys_time++;
	free_pos=find_free_pos(second_arr,MAP_SECOND_MAX_ENTRIES);
  if(free_pos==-1){
      printf("can not find free pos in second_arr\n");
      assert(0);
  }
	second_arr[free_pos]=blkno;
  MAP_SECOND_NUM_ENTRIES++;

  // write or read data page 
 	  if(operation==0){
			write_count++;
			MLC_opagemap[blkno].update = 1;
		}
	  else
			read_count++;

		send_flash_request(blkno*8, 8, operation, 1,1); 
	
	// debug test 
	if(MLC_opagemap[blkno].map_status!=MAP_SECOND){
		printf("not set MLC_opagemap flag\n");
		assert(0);
	}
	if(search_table(second_arr,MAP_SECOND_MAX_ENTRIES,blkno)==-1){
		printf("not play lpn-entry:%d into CMT\n");
		assert(0);
	}
		
}

int MLC_find_second_min()
{
  int i,index; 
  int temp = 99999999;

  for(i=0; i < MAP_SECOND_MAX_ENTRIES; i++) {
    if(second_arr[i]>0){
        if(MLC_opagemap[second_arr[i]].map_age <= temp) {
            second_min = second_arr[i];
            temp = MLC_opagemap[second_arr[i]].map_age;
            index = i;
        }
    }

  }
  return second_min;  
}

// C-CMT之后要修改
void C_CMT_Is_Full()
{
    // int min_second=-1;
    // min_second=MLC_find_second_min();
	int i=0;
	int offset=0;
    // 若果满了,先选择关联度最大进行删除
    if(MAP_SECOND_MAX_ENTRIES-MAP_SECOND_NUM_ENTRIES==0){
      MC=0;
      find_MC_entries(second_arr,MAP_SECOND_MAX_ENTRIES);
      send_flash_request(maxentry*8,8,1,2,1);
      translation_read_num++;
      send_flash_request(maxentry*8,8,0,2,1);
      translation_write_num++;

			//debug test
		for(i=0;i<MLC_MAP_ENTRIES_PER_PAGE;i++){
			offset=i+MLC_page_num_for_2nd_map_table+maxentry*MLC_MAP_ENTRIES_PER_PAGE;
			if(MLC_opagemap[offset].map_status==MAP_SECOND){
				if(search_table(second_arr,MAP_SECOND_MAX_ENTRIES,offset)==-1){
					printf("before CCMT delete is failded\n");
					assert(0);
				}
			}
		}
	

      
      //sencond_arr数组里面存的是lpn,将翻译页关联的映射项全部置为无效
      for(indexold = 0;indexold < MAP_SECOND_MAX_ENTRIES; indexold++){
          if(((second_arr[indexold]-MLC_page_num_for_2nd_map_table)/MLC_MAP_ENTRIES_PER_PAGE) == maxentry){
            MLC_opagemap[second_arr[indexold]].update = 0;
            MLC_opagemap[second_arr[indexold]].map_status = MAP_INVALID;
            MLC_opagemap[second_arr[indexold]].map_age = 0;
            second_arr[indexold]=0;
            MAP_SECOND_NUM_ENTRIES--;
          }
		}

		
		//debug test
		for(i=0;i<MLC_MAP_ENTRIES_PER_PAGE;i++){
			offset=i+MLC_page_num_for_2nd_map_table+maxentry*MLC_MAP_ENTRIES_PER_PAGE;
			if(MLC_opagemap[offset].map_status==MAP_SECOND){
				if(search_table(second_arr,MAP_SECOND_MAX_ENTRIES,offset)==-1){
					printf("after CCMT delete is failded\n");
					assert(0);
				}
			}
		}

        
        
    }
}


void load_CCMT_or_SCMT_to_HCMT(int blkno,int operation)
{
  int pos=-1,free_pos=-1;

  if(MLC_opagemap[blkno].map_status==MAP_SECOND){
    pos=search_table(second_arr,MAP_SECOND_MAX_ENTRIES,blkno);
    if(pos==-1){
      printf("can not find blkno :%d in second_arr\n",blkno);
      assert(0);
    }
    second_arr[pos]=0;
    MAP_SECOND_NUM_ENTRIES--;
    free_pos=find_free_pos(real_arr,MAP_REAL_MAX_ENTRIES);
    if(free_pos==-1){
      printf("can not find free pos in real_arr\n");
      assert(0);
    }
    real_arr[free_pos]=blkno;
    MLC_opagemap[blkno].map_status=MAP_REAL;
    MAP_REAL_NUM_ENTRIES++;

  }else if(MLC_opagemap[blkno].map_status==MAP_SEQ){
    // seq的最大时间是根据real
    // seq命中的映射项不迁移，保留等待被覆盖
    //~ pos=search_table(seq_arr,MAP_SEQ_MAX_ENTRIES,blkno);
    //~ if(pos==-1){
      //~ printf("can not find blkno :%d in seq_arr\n",blkno);
      //~ assert(0);
    //~ }
    //~ seq_arr[pos]=0;
    //~ MAP_SEQ_NUM_ENTRIES--;
    free_pos=find_free_pos(real_arr,MAP_REAL_MAX_ENTRIES);
                  if(free_pos==-1){
      printf("can not find free pos in real_arr\n");
      assert(0);
    }
    real_arr[free_pos]=blkno;
    MLC_opagemap[blkno].map_status=MAP_REAL;
    MAP_REAL_NUM_ENTRIES++;
  }else{
    //printf("should not come here\n");
    //assert(0);
  }

  //operation data pages 
  if(operation==0){
    write_count++;
    MLC_opagemap[blkno].update = 1;
  }
  else
    read_count++;

  send_flash_request(blkno*8, 8, operation, 1,1); 

}

