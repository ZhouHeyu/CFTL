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


//导入运算时间测试内联函数
#if defined (__i386__)
static __inline__ unsigned long long GetCycleCount(void)
{
        unsigned long long int x;
        __asm__ volatile("rdtsc":"=A"(x));
        return x;
}
#elif defined (__x86_64__)
static __inline__ unsigned long long GetCycleCount(void)
{
  unsigned hi,lo;
  __asm__ volatile("rdtsc":"=a"(lo),"=d"(hi));
  return ((unsigned long long)lo)|(((unsigned long long)hi)<<32);
}
#endif

//假设CPU为2GHz,换算时间为ms
#define FREQUENCY 2000000

//运行测试时间的变量
unsigned long t1,t2;

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
 int operation_time=0;
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

int zhou_flag=0;

/***************新的预热函数*************************/
void FTL_Warm(int *pageno,int *req_size,int operation);

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
//void load_CCMT_or_SCMT_to_HCMT(int blkno,int operation);
void move_CCMT_to_HCMT(int req_lpn,int operation);
void move_SCMT_to_HCMT(int blkno ,int operation);
void CPFTL_pre_load_entry_into_SCMT(int *pageno,int *req_size,int operation);

/********************************************************
 *         author:zhoujie     ADFTL
 * *******************************************************/
int ADFTL_WINDOW_SIZE=0;
double ADFTL_Tau=0.3;
int warm_flag;

void ADFTL_Scheme(int *pageno,int *req_size,int operation,int flash_flag);
void ADFTL_init_arr();
void ADFTL_Hit_R_CMT(int blkno,int operation);
void ADFTL_Cluster_CMT_Is_Full();
void ADFTL_R_CMT_Is_Full();
void ADFTL_Move_Cluster_CMT_to_RCMT(int req_lpn,int operation);
void ADFTL_Move_SCMT_to_RCMT(int blkno,int operation);
void load_entry_into_R_CMT(int blkno,int operation);
void ADFTL_pre_load_entry_into_SCMT(int *pageno,int *req_size,int operation);
void ADFTL_Read_Hit_ClusterCMT_or_SCMT(int blkno,int operation);
/***********ADFTL R-CMT is Full 涉及封装的函数***********************/
int Find_Min_Insert_pos(int * arr,int size,int value);
void Insert_Value_In_Arr(int *arr,int size,int pos,int value);
int ADFTL_Find_Victim_In_RCMT_W();
//根据双链表操作快速找到RCMT W窗口内的置换项剔除
int  Fast_Find_Victim_In_RCMT_W();
Node *ADFTL_Head=NULL;



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
  }
  MAP_REAL_NUM_ENTRIES=0;
  MAP_SECOND_NUM_ENTRIES=0;
  MAP_SEQ_NUM_ENTRIES=0;


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
		  DFTL_Scheme(&blkno,&cnt,operation,flash_flag);
		// CPFTL scheme
		 //     CPFTL_Scheme(&blkno,&cnt,operation,flash_flag);
            // ADFTL scheme
           // ADFTL_Scheme(&blkno,&cnt,operation,flash_flag);
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


void FTL_Warm(int *pageno,int *req_size,int operation)
{
//    首先是翻译页的读取,不经过CMT，直接进行预热
  int blkno=(*pageno),cnt=(*req_size);
  if(operation==0){
    send_flash_request(((blkno-MLC_page_num_for_2nd_map_table)/MLC_MAP_ENTRIES_PER_PAGE)*8,8,1,2,1);
    translation_read_num++;
    send_flash_request(((blkno-MLC_page_num_for_2nd_map_table)/MLC_MAP_ENTRIES_PER_PAGE)*8,8,0,2,1);
    translation_write_num++;
  } else{
    send_flash_request(((blkno-MLC_page_num_for_2nd_map_table)/MLC_MAP_ENTRIES_PER_PAGE)*8,8,1,2,1);
    translation_read_num++;
  }


//    其次数据页的读取
  sequential_count = 0;
  //之后连续的请求因为映射加载完成直接读取写入操作
  for(;(cnt>0)&&(sequential_count<NUM_ENTRIES_PER_TIME);cnt--)
  {
    if(operation==0)
    {
      write_count++;
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
        if (itemcount==itemcount_threshold&&zhou_flag==0){
          //重要的初始化在此初始化
          request_cnt = rqst_cnt;
          write_cnt = write_count;
          read_cnt = read_count;
          write_ratio = (write_cnt*1.0)/request_cnt;//写请求比例
          read_ratio = (read_cnt*1.0)/request_cnt;  //读请求比列 
          average_request_size = (total_request_size*1.0)/itemcount;//请求平均大小
            // CMT value size is 64KB real_arr is 32KB include entry(512 in 2Kpage)
          MAP_REAL_MAX_ENTRIES=8192;
          real_arr=(int *)malloc(sizeof(int)*MAP_REAL_MAX_ENTRIES);
            // ghost_arr is 32KB ,8192 entries
          MAP_GHOST_MAX_ENTRIES=8192;
          ghost_arr=(int *)malloc(sizeof(int)*MAP_GHOST_MAX_ENTRIES);
          DFTL_init_arr();
            zhou_flag=1;
        }
        
        rqst_cnt++;
//        预热函数FTL_warm
        if(warm_flag==1){
          FTL_Warm(&blkno,&cnt,operation);
        }else{
//          结束预热仿真
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

        if (itemcount==itemcount_threshold && zhou_flag==0){
          request_cnt = rqst_cnt;
          write_cnt = write_count;
          read_cnt = read_count;
          write_ratio = (write_cnt*1.0)/request_cnt;//写请求比例
          read_ratio = (read_cnt*1.0)/request_cnt;  //读请求比列 
          
          average_request_size = (total_request_size*1.0)/itemcount;//请求平均大小
            // CMT 32KB include entry num 8192
          MAP_REAL_MAX_ENTRIES=8129;
          real_arr=(int *)malloc(sizeof(int)*MAP_REAL_MAX_ENTRIES);
          //MAP_GHOST_MAX_ENTRIES=822;
          //ghost_arr=(int *)malloc(sizeof(int)*MAP_GHOST_MAX_ENTRIES);
            // SCMT 12KB include entry num 3072
          MAP_SEQ_MAX_ENTRIES=3072;
          seq_arr=(int *)malloc(sizeof(int)*MAP_SEQ_MAX_ENTRIES);
            // SLCMT 20KB include entry num 5120
          MAP_SECOND_MAX_ENTRIES=5120;
          second_arr=(int *)malloc(sizeof(int)*MAP_SECOND_MAX_ENTRIES); 
          init_arr();
            zhou_flag=1;
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
					assert(0);
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

        if (itemcount==itemcount_threshold&&zhou_flag==0){
//            为了配合warm时候的CMT已经加载，所以需要一个标识符zhou_flag来跳过这个初始化函数
            request_cnt = rqst_cnt;
            write_cnt = write_count;
            read_cnt = read_count;
            write_ratio = (write_cnt*1.0)/request_cnt;//写请求比例
            read_ratio = (read_cnt*1.0)/request_cnt;  //读请求比列
          
            average_request_size = (total_request_size*1.0)/itemcount;//请求平均大小

            //H-CMT 28KB page(2kB 512entries) 7168
            MAP_REAL_MAX_ENTRIES=7168;
          // real_arr 当做H-CMT使用
            real_arr=(int *)malloc(sizeof(int)*MAP_REAL_MAX_ENTRIES);
            // S-CMT 16KB 4096
            MAP_SEQ_MAX_ENTRIES=4096;
          // seq_arr当做S-CMT使用 
            seq_arr=(int *)malloc(sizeof(int)*MAP_SEQ_MAX_ENTRIES);
            //C-CMT 20kB  5120
            MAP_SECOND_MAX_ENTRIES=5120;
          // second_arr当做C-CMT使用 
            second_arr=(int *)malloc(sizeof(int)*MAP_SECOND_MAX_ENTRIES);
            init_arr();
            zhou_flag=1;
        }
          rqst_cnt++;
          // 此处开始CPFTL函数的逻辑
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
//			if(MLC_opagemap[blkno].map_status==MAP_SECOND){
//				if(search_table(second_arr,MAP_SECOND_MAX_ENTRIES,blkno)==-1){
//					printf("before second arr is error\n");
//					assert(0);
//				}
//			}

              if(MLC_opagemap[blkno].map_status==MAP_SECOND){
                  move_CCMT_to_HCMT(blkno,operation);

              }else if(MLC_opagemap[blkno].map_status==MAP_SEQ){
                  move_SCMT_to_HCMT(blkno,operation);
              }else{
                  printf("should not come here!\n");
                  assert(0);

              }


            //debug test
//			if(MLC_opagemap[blkno].map_status==MAP_SECOND){
//				if(search_table(second_arr,MAP_SECOND_MAX_ENTRIES,blkno)==-1){
//					printf("after second arr is error\n");
//					assert(0);
//				}
//			}

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
	MLC_opagemap[blkno].map_age=operation_time;
	operation_time++;
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
							printf("not reset min_real:%d into second_arr\n",min_real);
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
				MLC_opagemap[blkno+indexofseq].map_age=operation_time;
				operation_time++;
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

  MLC_opagemap[blkno].map_age=operation_time;
  operation_time++;
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
		printf("not play lpn-entry:%d into CMT\n",blkno);
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
//	int offset=0;
    // 若果满了,先选择关联度最大进行删除
    if(MAP_SECOND_MAX_ENTRIES-MAP_SECOND_NUM_ENTRIES==0){
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
}

// 因为CPFTL命中CCMT加载到H-CMT中的逻辑存在混乱，故重新定义一个加载函数
void move_CCMT_to_HCMT(int req_lpn,int operation)
{
    int flag=-1,real_min,pos=-1,pos_2nd=-1,free_pos=-1;
//    int temp;
    int limit_start=-1,limit_end=-1;

    //debug-value
    int last_second;
    int last_real;


    pos_2nd=search_table(second_arr,MAP_SECOND_MAX_ENTRIES,req_lpn);

    MC=0;
    find_MC_entries(second_arr,MAP_SECOND_MAX_ENTRIES);
    limit_start=maxentry*MLC_MAP_ENTRIES_PER_PAGE+MLC_page_num_for_2nd_map_table;
    limit_end=(maxentry+1)*MLC_MAP_ENTRIES_PER_PAGE+MLC_page_num_for_2nd_map_table;
    if(req_lpn>=limit_start && req_lpn<=limit_end){
        flag=1;
    }
    //处理掉特殊情况
    if(flag!=-1 && MAP_REAL_NUM_ENTRIES == MAP_REAL_MAX_ENTRIES){
        //printf("real is full and CCMT hit need to load entry to  HCMT\n");
        real_min=MLC_find_real_min();
        if(MLC_opagemap[real_min].update==1){
            // 直接交换两者的位置
            pos = search_table(real_arr,MAP_REAL_MAX_ENTRIES,real_min);
            MLC_opagemap[real_min].map_status = MAP_SECOND;
            second_arr[pos_2nd]=real_min;
            real_arr[pos]=req_lpn;
            MLC_opagemap[req_lpn].map_status=MAP_REAL;
            MLC_opagemap[req_lpn].map_age=operation_time;
            operation_time++;
        }else{
            // 没有更新直接剔除到flash
            pos = search_table(real_arr,MAP_REAL_MAX_ENTRIES,real_min);
            real_arr[pos]=0;
            MLC_opagemap[real_min].map_status = MAP_INVALID;
            MLC_opagemap[real_min].map_age = 0;
            MAP_REAL_NUM_ENTRIES--;
            //将real_arr[pos]存放刚刚命中的lpn
            real_arr[pos]=req_lpn;
            MLC_opagemap[req_lpn].map_status=MAP_REAL;
            MAP_REAL_NUM_ENTRIES++;
            MLC_opagemap[req_lpn].map_age=operation_time;
            operation_time++;
            second_arr[pos_2nd]=0;
            MAP_SECOND_NUM_ENTRIES--;

        }
        //特殊情况处理完毕
    }else{
        //其他情况依旧采用之前的置换策略；
        last_real=MAP_REAL_NUM_ENTRIES;
        last_second=MAP_SECOND_NUM_ENTRIES;
        H_CMT_Is_Full();
        pos=search_table(second_arr,MAP_SECOND_MAX_ENTRIES,req_lpn);
        if(pos==-1){
            printf("can not find blkno :%d in second_arr\n",req_lpn);
            assert(0);
        }
        second_arr[pos]=0;
        MAP_SECOND_NUM_ENTRIES--;
        free_pos=find_free_pos(real_arr,MAP_REAL_MAX_ENTRIES);
        if(free_pos==-1){
            printf("can not find free pos in real_arr\n");
            assert(0);
        }
        real_arr[free_pos]=req_lpn;
        MAP_REAL_NUM_ENTRIES++;
        MLC_opagemap[req_lpn].map_status=MAP_REAL;
        MLC_opagemap[req_lpn].map_age=operation_time;
        operation_time++;

    }
    //data page operation
    if(operation==0){
        write_count++;
        MLC_opagemap[req_lpn].update = 1;
    }
    else
        read_count++;

    send_flash_request(req_lpn*8, 8, operation, 1,1);

}


void move_SCMT_to_HCMT(int blkno ,int operation)
{
    int free_pos=-1;
    H_CMT_Is_Full();
    free_pos=find_free_pos(real_arr,MAP_REAL_MAX_ENTRIES);
    if(free_pos==-1){
        printf("can not find free pos in real_arr\n");
        assert(0);
    }
    real_arr[free_pos]=blkno;
    MLC_opagemap[blkno].map_status=MAP_REAL;
    MLC_opagemap[blkno].map_age=operation_time;
    operation_time++;
    MAP_REAL_NUM_ENTRIES++;
    //operation data pages
    if(operation==0){
        write_count++;
        MLC_opagemap[blkno].update = 1;
    }
    else
        read_count++;

    send_flash_request(blkno*8, 8, operation, 1,1);

}

/**************************************
 *  FTL alogrithm ADFTL
 * @param pageno
 * @param req_size
 * @param operation
 * @param flash_flag
 */

void ADFTL_Scheme(int *pageno,int *req_size,int operation,int flash_flag)
{
    int blkno=(*pageno),cnt=(*req_size);

    unsigned long t1,t2;
    Node *Temp;

    int pos=-1,free_pos=-1;

    if(flash_flag==0){
        send_flash_request(blkno*4, 4, operation, 1,0);
        blkno++;
    }
    else {

        if (itemcount < itemcount_threshold) {
            //利用trace数进行判断
            rqst_cnt++;
            if (operation == 0) {
                write_count++;//用于计算总的写请求数
            } else
                read_count++;
            blkno++;
        } else {

            if (itemcount == itemcount_threshold && zhou_flag == 0) {
//            为了配合warm时候的CMT已经加载，所以需要一个标识符zhou_flag来跳过这个初始化函数
                request_cnt = rqst_cnt;
                write_cnt = write_count;
                read_cnt = read_count;
                write_ratio = (write_cnt * 1.0) / request_cnt;//写请求比例
                read_ratio = (read_cnt * 1.0) / request_cnt;  //读请求比列

                average_request_size = (total_request_size * 1.0) / itemcount;//请求平均大小

                //H-CMT 28KB page(2kB 512entries) 7168
                MAP_REAL_MAX_ENTRIES = 7168;
                // real_arr 当做R-CMT使用
                real_arr = (int *) malloc(sizeof(int) * MAP_REAL_MAX_ENTRIES);
                // S-CMT 16KB 4096 预取的8个,所以是8的倍数
                MAP_SEQ_MAX_ENTRIES = 4096;
                // seq_arr当做S-CMT使用
                seq_arr = (int *) malloc(sizeof(int) * MAP_SEQ_MAX_ENTRIES);
                //Cluster-CMT 20kB  5120
                MAP_SECOND_MAX_ENTRIES = 5120;

                //RCMT的优先置换区的大小
                ADFTL_WINDOW_SIZE=(int)MAP_REAL_MAX_ENTRIES*ADFTL_Tau;
                // second_arr当做Cluster-CMT使用
                second_arr = (int *) malloc(sizeof(int) * MAP_SECOND_MAX_ENTRIES);
                ADFTL_Head=CreateList();
                ADFTL_init_arr();


                zhou_flag = 1;
            }
            rqst_cnt++;
            /*********AD-FTL 逻辑处理*****************/

            if(warm_flag==1){
//                预热阶段，全部采用预取方式：
                FTL_Warm(&blkno,&cnt,operation);

            }else{
//                非预热，仿真运行阶段
                if(MLC_opagemap[blkno].map_status==MAP_REAL){
                    //命中R-CMT
                    if(ListLength(ADFTL_Head)!=MAP_REAL_NUM_ENTRIES){
                        printf(" before ADFTL Hit R CMT error,ListLength is %d,real_arr size is %d\n",ListLength(ADFTL_Head),MAP_REAL_NUM_ENTRIES);
                        assert(0);
                    }

                    ADFTL_Hit_R_CMT(blkno,operation);
                    if(ListLength(ADFTL_Head)!=MAP_REAL_NUM_ENTRIES){
                        printf(" after ADFTL Hit R CMT error,ListLength is %d,real_arr size is %d\n",ListLength(ADFTL_Head),MAP_REAL_NUM_ENTRIES);
                        assert(0);
                    }

                }else if(MLC_opagemap[blkno].map_status==MAP_SECOND || MLC_opagemap[blkno].map_status==MAP_SEQ){

                    operation_time++;
                    //只有写请求才可以移动到R-CMT中
                    if(operation==0){
                        if(MLC_opagemap[blkno].map_status==MAP_SECOND){
                            ADFTL_Move_Cluster_CMT_to_RCMT(blkno,operation);

                            if(ListLength(ADFTL_Head)!=MAP_REAL_NUM_ENTRIES){
                                printf(" after ADFTL_Move_Cluster_CMT_to_RCMT error,ListLength is %d,real_arr size is %d\n",ListLength(ADFTL_Head),MAP_REAL_NUM_ENTRIES);
                                assert(0);
                            }
                        }else if(MLC_opagemap[blkno].map_status==MAP_SEQ){
                            ADFTL_Move_SCMT_to_RCMT(blkno,operation);

                            if(ListLength(ADFTL_Head)!=MAP_REAL_NUM_ENTRIES){
                                printf(" after ADFTL_Move_SCMT_to_RCMT error,ListLength is %d,real_arr size is %d\n",ListLength(ADFTL_Head),MAP_REAL_NUM_ENTRIES);
                                assert(0);
                            }

                        }else{
                            //debug print
                            printf("can not exist this happend\n");
                            assert(0);
                        }
                    }else{
                        //读请求命中项不做转移操作，但需要做数据访问请求处理
                        ADFTL_Read_Hit_ClusterCMT_or_SCMT(blkno,operation);
                    }


                } else if((cnt+1)>=THRESHOLD){
                    // 预取策略
                    ADFTL_pre_load_entry_into_SCMT(&blkno,&cnt,operation);

                    if(ListLength(ADFTL_Head)!=MAP_REAL_NUM_ENTRIES){
                        printf(" after ADFTL_pre_load_entry_into_SCMT error,ListLength is %d,real_arr size is %d\n",ListLength(ADFTL_Head),MAP_REAL_NUM_ENTRIES);
                        assert(0);
                    }


                } else{
                    //第一次加载的数据到R-CMT中
                    ADFTL_R_CMT_Is_Full();
                    load_entry_into_R_CMT(blkno,operation);
                    if(ListLength(ADFTL_Head)!=MAP_REAL_NUM_ENTRIES){
                        printf(" after 第一次加载的数据到R-CMT中 error,ListLength is %d,real_arr size is %d\n",ListLength(ADFTL_Head),MAP_REAL_NUM_ENTRIES);
                        assert(0);
                    }
                }

            }

        }

    }

    //syn-value
    (*pageno)=blkno,(*req_size)=cnt;

}


void ADFTL_init_arr()
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
    }
    MAP_REAL_NUM_ENTRIES = 0;
    MAP_SECOND_NUM_ENTRIES = 0;
    MAP_SEQ_NUM_ENTRIES = 0;

}

/***************************************
 * ADFTL的封装函数(命中R-CMT)
 * @param blkno
 * @param operation
 */

void ADFTL_Hit_R_CMT(int blkno,int operation)
{
    Node *temp;
    MLC_opagemap[blkno].map_status=MAP_REAL;
    MLC_opagemap[blkno].map_age=operation_time;
    operation_time++;

//    操作链表的LRU
    temp=SearchLPNInList(blkno,ADFTL_Head);
    if(temp==NULL){
      printf("error in ADFTL_Hit_R_CMT,can not find blkno %d in List\n",blkno);
      assert(0);
    } else{
//      move node
      InsertNodeInListMRU(temp,ADFTL_Head);
    }

    // write or read data page
    if(operation==0){
        write_count++;
        MLC_opagemap[blkno].update = 1;
    }
    else
        read_count++;

    send_flash_request(blkno*8, 8, operation, 1,1);

}

void ADFTL_Read_Hit_ClusterCMT_or_SCMT(int blkno,int operation)
{

  MLC_opagemap[blkno].map_age=operation_time;

  // write or read data page
  if(operation==0){
    write_count++;
    MLC_opagemap[blkno].update = 1;
  }
  else
    read_count++;

  send_flash_request(blkno*8, 8, operation, 1,1);
}


/**************************************
 *  当Cluster_CMT满的时候按照最大簇原则选择回写
 */

void ADFTL_Cluster_CMT_Is_Full()
{
    int i=0;
//	int offset=0;
    // 若果满了,先选择关联度最大进行删除
    if(MAP_SECOND_MAX_ENTRIES-MAP_SECOND_NUM_ENTRIES==0){
      //debug time
//      printf("start Cluster CMT is Full\n");
//      t1=(unsigned long) GetCycleCount();

        MC=0;
        find_MC_entries(second_arr,MAP_SECOND_MAX_ENTRIES);
        send_flash_request(maxentry*8,8,1,2,1);
        translation_read_num++;
        send_flash_request(maxentry*8,8,0,2,1);
        translation_write_num++;

        //sencond_arr数组里面存的是lpn,将翻译页关联的映射项全部置为无效
        for(indexold = 0;indexold < MAP_SECOND_MAX_ENTRIES; indexold++){
            //后续这里可以添加当Cluster簇回写的时候，将R-CMT的映射关系同步更新下，降低回写次数
            if(((second_arr[indexold]-MLC_page_num_for_2nd_map_table)/MLC_MAP_ENTRIES_PER_PAGE) == maxentry){
                MLC_opagemap[second_arr[indexold]].update = 0;
                MLC_opagemap[second_arr[indexold]].map_status = MAP_INVALID;
                MLC_opagemap[second_arr[indexold]].map_age = 0;
                second_arr[indexold]=0;
                MAP_SECOND_NUM_ENTRIES--;
            }
        }


//      t2=(unsigned long) GetCycleCount();
//      printf("Cluster CMT is Full -Use Time:%f\n",(t2 - t1)*1.0/FREQUENCY);

    }

}

/*************ADFTL的RCMT的剔除操作略显复杂***************************/
//该函数完成的是对arr数组中值和value的对比，默认arr数组中的数据是按上升排列的
//选择比value值刚好大的数据前一个位置插入，(arr[x]<value<arr[y]),replace y
int Find_Min_Insert_pos(int * arr,int size,int value)
{
    int pos=-1;
    int i;
    for(i=0;i<size;i++){
        if(arr[i]>=value){
            pos=i;
            break;
        }
    }
    return pos;
}

//该函数完成将value值插入到arr数组中，同时将pos-end-1的数组移动到pos+1-end的位置覆盖
void Insert_Value_In_Arr(int *arr,int size,int pos,int value)
{
    int i;
    //debug
    if(pos<0 || pos>=size){
        printf("pos :%d is over arr_size:%d!\n",pos,size);
        assert(0);
    }
    //刚好位于尾部的时候
    if(pos==size-1){
        arr[pos]=value;
    }else{
        //首先数据覆盖迁移
        for(i=size-1;i>pos;i--){
            arr[i]=arr[i-1];
        }
        arr[pos]=value;
    }

}


//根据双链表操作快速找到RCMT W窗口内的置换项剔除
int  Fast_Find_Victim_In_RCMT_W()
{
  Node *Temp;
  int i,pos_index,Victim_index,curr_lpn,clean_flag=0;
//  从尾部进行扫描,优先找到干净项进行删除
  Temp=ADFTL_Head->pre;
  for(i=0;i<ADFTL_WINDOW_SIZE;i++){
    Temp=Temp->pre;
    curr_lpn=Temp->lpn_num;
    if(MLC_opagemap[curr_lpn].update==0 && MLC_opagemap[curr_lpn].map_status==MAP_REAL){
      clean_flag=1;
      break;
    }
  }

  if(clean_flag==0){
//      选择LRU位置的脏映射项
    curr_lpn=ADFTL_Head->pre->lpn_num;
    Victim_index=search_table(real_arr,MAP_REAL_MAX_ENTRIES,curr_lpn);
    if(Victim_index==-1){
      printf("can not find LRU pos lpn %d in real_arr\n",curr_lpn);
      assert(0);
    }

  }else{
//    选择窗口内的干净页映射项
    if(MLC_opagemap[curr_lpn].update!=0){
      printf("error MLC_opagemap[%d]->update can not be update\n",curr_lpn);
      assert(0);
    }
    Victim_index=search_table(real_arr,MAP_REAL_MAX_ENTRIES,curr_lpn);
    if(Victim_index==-1){
      printf("can not find LRU pos lpn %d in real_arr\n",curr_lpn);
      assert(0);
    }
  }

  return Victim_index;


}

//涉及到根据operation_time对second_arr的元素进行排序的问题
//后期需要代码优化,函数返回的是置换的映射项在real_arr的位置
int ADFTL_Find_Victim_In_RCMT_W()
{
    int i,clean_flag=0,pos_index=-1,temp_time,Victim_index;
    
	unsigned long S1,S2;
    //debug time
    unsigned long T1,T2;

	
    int *index_arr,*lpn_arr,*Time_arr;
    //32位最大的int值
    int Temp_MAXTIME=999999999;

    
	S1=(unsigned long)GetCycleCount();
    //debug
    if(operation_time>=Temp_MAXTIME){
        printf("operation_time is %d,over MAXTIME\n",operation_time);
        assert(0);
    }



    //存的是real_arr的下标索引
    index_arr=(int *)malloc(ADFTL_WINDOW_SIZE*sizeof(int));
    lpn_arr=(int *)malloc(ADFTL_WINDOW_SIZE*sizeof(int));
    Time_arr=(int *)malloc(ADFTL_WINDOW_SIZE*sizeof(int));

    //init
    for(i=0;i<ADFTL_WINDOW_SIZE;i++){
        index_arr[i]=-1;
        Time_arr[i]=Temp_MAXTIME;
        lpn_arr[i]=-1;
    }

     //    debug time
    T1=(unsigned long)GetCycleCount();

    //根据operatintime做升序排序（最前面就是越靠近LRU）
    for(i=0;i<MAP_REAL_MAX_ENTRIES;i++){
        temp_time=MLC_opagemap[real_arr[i]].map_age;
        pos_index=Find_Min_Insert_pos(Time_arr,ADFTL_WINDOW_SIZE,temp_time);

//        debug time
        T2=(unsigned long)GetCycleCount();
        if(((T2 - T1)*1.0/FREQUENCY)>1){
	        printf("Find_Min_Insert_pos (Find Victim_In_RCMT_W) -Use Time:%f\n",(T2 - T1)*1.0/FREQUENCY);
		}

        T1=T2;

        //存在可插入的最小值位置
        if(pos_index!=-1){
            Insert_Value_In_Arr(Time_arr,ADFTL_WINDOW_SIZE,pos_index,temp_time);
            Insert_Value_In_Arr(index_arr,ADFTL_WINDOW_SIZE,pos_index,i);
            Insert_Value_In_Arr(lpn_arr,ADFTL_WINDOW_SIZE,pos_index,real_arr[i]);
        }
//      debug time
        T2=(unsigned long)GetCycleCount();
        if(((T2 - T1)*1.0/FREQUENCY)>1){
			printf("Insert_Value_In_Arr (Find Victim_In_RCMT_W) -Use Time:%f\n",(T2 - T1)*1.0/FREQUENCY);
		}
        T1=T2;

    }



    for(i=0;i<ADFTL_WINDOW_SIZE;i++){
        if(MLC_opagemap[lpn_arr[i]].update==0){
            clean_flag=1;
            Victim_index=index_arr[i];
            break;
        }
    }

    if(clean_flag==0){
        Victim_index=index_arr[0];
    }

    S2=(unsigned long)GetCycleCount();

	printf("Find Victim_In_RCMT_W ALL -Use Time:%f\n",(S2 - S1)*1.0/FREQUENCY);


    return Victim_index;

}

void ADFTL_R_CMT_Is_Full()
{
    int Victim_pos=-1,free_pos=-1,curr_lpn;
    Node *Temp;
    //debug time
    unsigned long T1,T2;

    if(MAP_REAL_MAX_ENTRIES-MAP_REAL_NUM_ENTRIES==0){
        //先确保Cluster_CMT有空闲的位置
        if(MAP_SECOND_MAX_ENTRIES-MAP_SECOND_NUM_ENTRIES==0){
            ADFTL_Cluster_CMT_Is_Full();
        }

        Victim_pos=Fast_Find_Victim_In_RCMT_W();
        curr_lpn=real_arr[Victim_pos];
        if(MLC_opagemap[curr_lpn].update!=0){
            //表明当前的优先置换区没有干净映射项，则选择LRU尾部脏页到Cluster-CMT中
            MLC_opagemap[curr_lpn].map_status=MAP_SECOND;
            free_pos=find_free_pos(second_arr,MAP_SECOND_MAX_ENTRIES);
            if(free_pos==-1){
                printf("can not find free pos in second arr for curr_lpn %d\n",curr_lpn);
                assert(0);
            }
            second_arr[free_pos]=curr_lpn;
            real_arr[Victim_pos]=0;
//          脏映射项直接是直接删除LRU位置的
            Temp=DeleteLRUInList(ADFTL_Head);
            if(Temp->lpn_num!=curr_lpn){
              printf("delete lru arr Temp->lpn %d not equal curr-lpn %d\n",Temp->lpn_num,curr_lpn);
              assert(0);
            }

            MAP_REAL_NUM_ENTRIES--;
            MAP_SECOND_NUM_ENTRIES++;
        }else{
        //选择该干净映射项剔除
            MLC_opagemap[curr_lpn].map_status=MAP_INVALID;
            real_arr[Victim_pos]=0;
            MLC_opagemap[curr_lpn].map_age=0;
            MLC_opagemap[curr_lpn].update=0;
//           操作链表,删除被置换掉的节点
            Temp=SearchLPNInList(curr_lpn,ADFTL_Head);
            DeleteNodeInList(Temp,ADFTL_Head);
            MAP_REAL_NUM_ENTRIES--;
        }

    }

}

/*****************ADFTL的数据迁移****************************/
void ADFTL_Move_Cluster_CMT_to_RCMT(int req_lpn,int operation)
{

    int flag=-1,real_min,pos=-1,pos_2nd=-1,free_pos=-1;
    int Victim_pos;
//    int temp;
    int limit_start=-1,limit_end=-1;
    Node *Temp;
    //debug-value
    int last_second;
    int last_real;


    pos_2nd=search_table(second_arr,MAP_SECOND_MAX_ENTRIES,req_lpn);

    MC=0;
    find_MC_entries(second_arr,MAP_SECOND_MAX_ENTRIES);
    limit_start=maxentry*MLC_MAP_ENTRIES_PER_PAGE+MLC_page_num_for_2nd_map_table;
    limit_end=(maxentry+1)*MLC_MAP_ENTRIES_PER_PAGE+MLC_page_num_for_2nd_map_table;
    if(req_lpn>=limit_start && req_lpn<=limit_end){
        flag=1;
    }
    //处理掉特殊情况
    if(flag!=-1 && MAP_REAL_NUM_ENTRIES == MAP_REAL_MAX_ENTRIES){
        //debug print
        //printf("real is full and CCMT hit need to load entry to  HCMT\n");
        //确定R-CMT中的置换对象
        //优先选择置换区W内的干净映射项
//        Victim_pos=ADFTL_Find_Victim_In_RCMT_W();
        Victim_pos=Fast_Find_Victim_In_RCMT_W();
        real_min=real_arr[Victim_pos];
        pos = Victim_pos;
        if(MLC_opagemap[real_min].update==1){
            // 直接交换两者的位置
            MLC_opagemap[real_min].map_status = MAP_SECOND;
            second_arr[pos_2nd]=real_min;
            real_arr[pos]=req_lpn;
            MLC_opagemap[req_lpn].map_status=MAP_REAL;
            MLC_opagemap[req_lpn].map_age=operation_time;
            operation_time++;
//            删除链表中原来的节点,并将新的节点导入到MRU
            Temp=SearchLPNInList(real_min,ADFTL_Head);
            DeleteNodeInList(Temp,ADFTL_Head);
            AddNewLPNInMRU(req_lpn,ADFTL_Head);
        }else{
            // 没有更新直接剔除到flash
            real_arr[pos]=0;
            MLC_opagemap[real_min].map_status = MAP_INVALID;
            MLC_opagemap[real_min].map_age = 0;
            MAP_REAL_NUM_ENTRIES--;
            //将real_arr[pos]存放刚刚命中的lpn
            real_arr[pos]=req_lpn;
            MLC_opagemap[req_lpn].map_status=MAP_REAL;
            MAP_REAL_NUM_ENTRIES++;
            MLC_opagemap[req_lpn].map_age=operation_time;
            operation_time++;
            second_arr[pos_2nd]=0;
//            删除链表中原来的节点,并将新的节点导入到MRU
            Temp=SearchLPNInList(real_min,ADFTL_Head);
            DeleteNodeInList(Temp,ADFTL_Head);
            AddNewLPNInMRU(req_lpn,ADFTL_Head);
            MAP_SECOND_NUM_ENTRIES--;

        }
        //特殊情况处理完毕
    }else{
        //其他情况依旧采用之前的置换策略；
//        debug-value
//        last_real=MAP_REAL_NUM_ENTRIES;
//        last_second=MAP_SECOND_NUM_ENTRIES;
        ADFTL_R_CMT_Is_Full();
        pos=search_table(second_arr,MAP_SECOND_MAX_ENTRIES,req_lpn);
        if(pos==-1){
            printf("can not find blkno :%d in second_arr\n",req_lpn);
            assert(0);
        }
        second_arr[pos]=0;
        MAP_SECOND_NUM_ENTRIES--;
        free_pos=find_free_pos(real_arr,MAP_REAL_MAX_ENTRIES);
        if(free_pos==-1){
            printf("can not find free pos in real_arr\n");
            assert(0);
        }
        real_arr[free_pos]=req_lpn;
        MAP_REAL_NUM_ENTRIES++;
        MLC_opagemap[req_lpn].map_status=MAP_REAL;
        MLC_opagemap[req_lpn].map_age=operation_time;
        operation_time++;
//        插入新的节点进入
        AddNewLPNInMRU(req_lpn,ADFTL_Head);
    }

    //data page operation
    if(operation==0){
        write_count++;
        MLC_opagemap[req_lpn].update = 1;
    }
    else
        read_count++;

    send_flash_request(req_lpn*8, 8, operation, 1,1);
}

void ADFTL_Move_SCMT_to_RCMT(int blkno,int operation)
{
    int free_pos=-1;
    ADFTL_R_CMT_Is_Full();
    free_pos=find_free_pos(real_arr,MAP_REAL_MAX_ENTRIES);
    if(free_pos==-1){
        printf("can not find free pos in real_arr\n");
        assert(0);
    }
    real_arr[free_pos]=blkno;
    MLC_opagemap[blkno].map_status=MAP_REAL;
    MLC_opagemap[blkno].map_age=operation_time;
    operation_time++;

//    链表操作
    AddNewLPNInMRU(blkno,ADFTL_Head);
    MAP_REAL_NUM_ENTRIES++;
    //operation data pages
    if(operation==0){
        write_count++;
        MLC_opagemap[blkno].update = 1;
    }
    else
        read_count++;

    send_flash_request(blkno*8, 8, operation, 1,1);
}

void load_entry_into_R_CMT(int blkno,int operation)
{
    int free_pos=-1;
    flash_hit++;
    // read MVPN page
    send_flash_request(((blkno-MLC_page_num_for_2nd_map_table)/MLC_MAP_ENTRIES_PER_PAGE)*8, 8, 1, 2,1);   // read from 2nd mapping table
    translation_read_num++;
    MLC_opagemap[blkno].map_status = MAP_REAL;

    MLC_opagemap[blkno].map_age=operation_time;
    operation_time++;
    free_pos=find_free_pos(real_arr,MAP_REAL_MAX_ENTRIES);
    if(free_pos==-1){
        printf("can not find free pos in real_arr\n");
        assert(0);
    }
    real_arr[free_pos]=blkno;
//    链表操作
    AddNewLPNInMRU(blkno,ADFTL_Head);

    MAP_REAL_NUM_ENTRIES++;

    // write or read data page
    if(operation==0){
        write_count++;
        MLC_opagemap[blkno].update = 1;
    }
    else
        read_count++;

    send_flash_request(blkno*8, 8, operation, 1,1);

    // debug test
    if(MLC_opagemap[blkno].map_status!=MAP_REAL){
        printf("not set MLC_opagemap flag\n");
        assert(0);
    }
    if(search_table(real_arr,MAP_REAL_MAX_ENTRIES,blkno)==-1){
        printf("not play lpn-entry:%d into CMT\n",blkno);
        assert(0);
    }

    if(SearchLPNInList(blkno,ADFTL_Head)==NULL){
      printf("not Add blkno %d into List\n",blkno);
      assert(0);
    }

    if(ListLength(ADFTL_Head)!=MAP_REAL_NUM_ENTRIES){
      printf("List Length is %d and real_arr num is %d\n",ListLength(ADFTL_Head),MAP_REAL_NUM_ENTRIES);
      assert(0);
    }

}

/*****************ADFTL数据预取策略***********************/




void ADFTL_pre_load_entry_into_SCMT(int *pageno,int *req_size,int operation)
{
    int blkno=(*pageno),cnt=(*req_size);
    int pos=-1,free_pos=-1;

    if(not_in_cache(blkno)){
        //~满足之后连续的pre_num个数的映射项不在CMT中才预取
        if((MAP_SEQ_MAX_ENTRIES-MAP_SEQ_NUM_ENTRIES)==0){
            //~如果seq_arr满了
            for(indexofarr = 0;indexofarr < NUM_ENTRIES_PER_TIME;indexofarr++){
                //SEQ的替换策略是把SEQ的最前面几个映射项剔除出去，查看是否存在更新页，存在一个更新页，整个翻译页都得重新更新，update_flag置位
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
            MLC_opagemap[blkno+indexofseq].map_age=operation_time;
            operation_time++;
            //~对应的seq命中转移数据页应该不能删除原来的数据
            seq_arr[MAP_SEQ_NUM_ENTRIES] = (blkno+indexofseq);//加载到SEQ的映射项是放在SEQ尾部
            MAP_SEQ_NUM_ENTRIES++;
        }
        if(MAP_SEQ_NUM_ENTRIES > MAP_SEQ_MAX_ENTRIES)
        {
            printf("The sequential cache is overflow!\n");
            assert(0);
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
        //~反之只是将其一个映射项加载到R-CMT中 (根据前面的删选，只能是不存在CMT的映射项)
        ADFTL_R_CMT_Is_Full();
        load_entry_into_R_CMT(blkno,operation);
        blkno++;
        //zhoujie
        *req_size=cnt;
        *pageno=blkno;
    }
    //连续加载处理完成
}
