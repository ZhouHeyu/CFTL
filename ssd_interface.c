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
//int MAP_GHOST_MAX_ENTRIES=0;
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
int real_min = -1;
int real_max = 0;
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
/***********************************************************************
  Cache
 ***********************************************************************/
int cache_min = -1;
int cache_max = 0;

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
        if(MLC_opagemap[real_arr[i]].map_age <= temp) {
            real_min = real_arr[i];
            temp = MLC_opagemap[real_arr[i]].map_age;
            index = i;
        }
  }
  return real_min;    
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
      cache_arr[i] = -1;
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

    printf("shouldnt come here for search_table()=%d,%d",val,size);
    for( i = 0; i < size; i++) {
      if(arr[i] != 0) {
        printf("arr[%d]=%d ",i,arr[i]);
      }
    }
    exit(1);
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
    printf("shouldn't come here for find_free_pos()\n");
    exit(1);
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

  switch(operation)
  {
    //write/read
    case 0:
    case 1:

    while(cnt > 0)
    {
          cnt--;
          itemcount++;
        // page based FTL
        if(ftl_type == 1){
          send_flash_request(blkno*4, 4, operation, 1,0); 
          blkno++;
        }

        // blck based FTL
        else if(ftl_type == 2){
          send_flash_request(blkno*4, 4, operation, 1,0); 
          blkno++;
        }

        // opagemap ftl scheme
        else if(ftl_type == 3)
        {
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
              else
              {
                if (itemcount==itemcount_threshold)
                {
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
                if (MLC_opagemap[blkno].map_status == MAP_REAL)
                {
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
  }


  //   while(cnt > 0)
  // {
  //     cnt--;
  //     itemcount++;
  //     switch(ftl_type){
  //       case 1:
  //             // page based FTL
  //             send_flash_request(blkno*4, 4, operation, 1,0); 
  //               blkno++;
  //             break;
  //       case 2:
  //             // blck based FTL
  //             send_flash_request(blkno*4, 4, operation, 1,0); 
  //               blkno++;
  //             break;
  //       case 4: 
  //             // FAST scheme 
  //             if(operation == 0){
  //               write_count++;
  //               }
  //               else {
  //                 read_count++;
  //               }
  //               send_flash_request(blkno*4, 4, operation, 1,0); //cache_min is a page for page baseed FTL
  //               blkno++;
  //             break;
  //       case 3:
  //             // SDFTL scheme
  //             SDFTL_Scheme(&blkno,&cnt,operation,flash_flag);
  //             break;
  //       }//end-switch

  // }//end-while


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

// {
//   			if(not_in_cache(blkno))
// 			{
// 			MLC_opagemap[blkno].map_age++;
// 			if((MAP_SEQ_MAX_ENTRIES-MAP_SEQ_NUM_ENTRIES)==0)
// 			{
// 				for(indexofarr = 0;indexofarr < NUM_ENTRIES_PER_TIME;indexofarr++)
// 				{//SEQ的替换策略是把SEQ的最前面几个映射项剔除出去
// 					if((MLC_opagemap[seq_arr[indexofarr]].update == 1)&&(MLC_opagemap[seq_arr[indexofarr]].map_status == MAP_SEQ))
// 					{
// 						//update_reqd++;
// 						update_flag=1;
// 						MLC_opagemap[seq_arr[indexofarr]].update=0;
// 						MLC_opagemap[seq_arr[indexofarr]].map_status = MAP_INVALID;
// 						MLC_opagemap[seq_arr[indexofarr]].map_age = 0;
// 					}
// 					else if((MLC_opagemap[seq_arr[indexofarr]].update == 0)&&(MLC_opagemap[seq_arr[indexofarr]].map_status ==MAP_SEQ))
// 					{
// 						MLC_opagemap[seq_arr[indexofarr]].map_status = MAP_INVALID;
// 						MLC_opagemap[seq_arr[indexofarr]].map_age = 0;
// 					}
// 				}
// 				if(update_flag == 1)
// 				{
// 					send_flash_request(((seq_arr[0]-MLC_page_num_for_2nd_map_table)/MLC_MAP_ENTRIES_PER_PAGE)*8,8,1,2,1);
// 					translation_read_num++;
// 					send_flash_request(((seq_arr[0]-MLC_page_num_for_2nd_map_table)/MLC_MAP_ENTRIES_PER_PAGE)*8,8,0,2,1);
// 					translation_write_num++;
// 					update_flag=0;
// 				}
// 				for(indexofarr = 0;indexofarr <= MAP_SEQ_MAX_ENTRIES-1-NUM_ENTRIES_PER_TIME; indexofarr++)
// 					seq_arr[indexofarr] = seq_arr[indexofarr+NUM_ENTRIES_PER_TIME];
// 				MAP_SEQ_NUM_ENTRIES-=NUM_ENTRIES_PER_TIME;
// 			}
// 			flash_hit++;
// 			send_flash_request(((blkno-MLC_page_num_for_2nd_map_table)/MLC_MAP_ENTRIES_PER_PAGE)*8, 8, 1, 2,1);
// 			translation_read_num++;
// 			for(indexofseq=0; indexofseq < NUM_ENTRIES_PER_TIME;indexofseq++)//NUM_ENTRIES_PER_TIME在这里表示一次加载4个映射表信息
// 			{
// 				MLC_opagemap[blkno+indexofseq].map_status=MAP_SEQ;
// 				seq_arr[MAP_SEQ_NUM_ENTRIES] = (blkno+indexofseq);//加载到SEQ的映射项是放在SEQ尾部
// 				MAP_SEQ_NUM_ENTRIES++;
// 			}
// 			if(MAP_SEQ_NUM_ENTRIES > MAP_SEQ_MAX_ENTRIES)
// 			{
// 				printf("The sequential cache is overflow!\n");
// 				exit(0);
// 			}
// 			if(operation==0)
// 			{
// 				write_count++;
// 				MLC_opagemap[blkno].update=1;
// 			}
// 			else
// 				read_count++;
// 			send_flash_request(blkno*8,8,operation,1,1);
			
//                      blkno++;
// 			sequential_count = 0;
// 			for(;(cnt>0)&&(sequential_count<NUM_ENTRIES_PER_TIME-1);cnt--)
// 			{
// 				MLC_opagemap[blkno].map_age++;
// 				cache_scmt_hit++;
// 				if(operation==0)
// 				{
// 					write_count++;
// 					MLC_opagemap[blkno].update=1;
// 				}
// 				else
// 					read_count++;
// 				send_flash_request(blkno*8,8,operation,1,1);
// 				blkno++;
// 				rqst_cnt++;
// 				sequential_count++;
// 			}
// 			continue;
// 			}
//            else
//                {
//               if((MAP_REAL_MAX_ENTRIES - MAP_REAL_NUM_ENTRIES) == 0)
//                  {
//                   min_real = MLC_find_real_min();
//                  if(MLC_opagemap[min_real].update == 1)
//                {
//                   	if((MAP_SECOND_MAX_ENTRIES-MAP_SECOND_NUM_ENTRIES) ==0)
// 		       {
// 				 MC=0;
// 				find_MC_entries(second_arr,MAP_SECOND_MAX_ENTRIES);
// 				send_flash_request(maxentry*8,8,1,2,1);
// 				translation_read_num++;
// 				send_flash_request(maxentry*8,8,0,2,1);
// 				translation_write_num++;
// 				for(indexold = 0;indexold < MAP_SECOND_MAX_ENTRIES; indexold++)
// 			     {
// 					if(((second_arr[indexold]-MLC_page_num_for_2nd_map_table)/MLC_MAP_ENTRIES_PER_PAGE) == maxentry)
// 				     {
// 						MLC_opagemap[second_arr[indexold]].update = 0;
// 						MLC_opagemap[second_arr[indexold]].map_status = MAP_INVALID;
// 						MLC_opagemap[second_arr[indexold]].map_age = 0;
// 						second_arr[indexold]=0;
// 						MAP_SECOND_NUM_ENTRIES--;
// 				    }
// 				}
// 			}
// 					MLC_opagemap[min_real].map_status = MAP_SECOND;
//                                    pos = search_table(real_arr,MAP_REAL_MAX_ENTRIES,min_real);
//                                    real_arr[pos]=0;
// 					MAP_REAL_NUM_ENTRIES--;
// 					pos_2nd = find_free_pos(second_arr,MAP_SECOND_MAX_ENTRIES);
// 					second_arr[pos_2nd]=0;
// 					second_arr[pos_2nd]=min_real;
// 					MAP_SECOND_NUM_ENTRIES++;
// 					if(MAP_SECOND_NUM_ENTRIES > MAP_SECOND_MAX_ENTRIES)
// 					{
// 						printf("The second cache is overflow!\n");
// 						exit(0);
// 					}

//                }
// 				else
// 				{
//                              pos = search_table(real_arr,MAP_REAL_MAX_ENTRIES,min_real);
//                              real_arr[pos]=0;
//                              MLC_opagemap[min_real].map_status = MAP_INVALID;
// 				 MLC_opagemap[min_real].map_age = 0;
//                              MAP_REAL_NUM_ENTRIES--;
// 				}

//             } 
//                 flash_hit++;
//             send_flash_request(((blkno-MLC_page_num_for_2nd_map_table)/MLC_MAP_ENTRIES_PER_PAGE)*8, 8, 1, 2,1);   // read from 2nd mapping table
// 			translation_read_num++;
//             MLC_opagemap[blkno].map_status = MAP_REAL;

//             MLC_opagemap[blkno].map_age = MLC_opagemap[real_max].map_age + 1;
//             real_max = blkno;
            
//             pos = find_free_pos(real_arr,MAP_REAL_MAX_ENTRIES);//因为real_arr已经被定义成指针变量，调用find_free_pos函数时前面不需要加*
//             real_arr[pos] = 0;
//             real_arr[pos] = blkno;
//             MAP_REAL_NUM_ENTRIES++;
//           if(operation==0){
//             write_count++;
//             MLC_opagemap[blkno].update = 1;
//           }
//           else
//              read_count++;

//              send_flash_request(blkno*8, 8, operation, 1,1); 
//                 blkno++;
//             continue;
//            }
// }