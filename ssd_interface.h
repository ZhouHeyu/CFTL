/* 
 * Contributors: Youngjae Kim (youkim@cse.psu.edu)
 *               Aayush Gupta (axg354@cse.psu.edu)
 *
 * In case if you have any doubts or questions, kindly write to: youkim@cse.psu.edu 
 *
 * Description: This is a header file for ssd_interface.c.
 *
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "fast.h"
#include "pagemap.h"
#include "flash.h"
#include "type.h"

//添加双链表的操作函数库
#include "List.h"

#define READ_DELAY        (0.1309/4)
#define WRITE_DELAY       (0.4059/4)
#define ERASE_DELAY       1.5 
#define GC_READ_DELAY  READ_DELAY    // gc read_delay = read delay    
#define GC_WRITE_DELAY WRITE_DELAY  // gc write_delay = write delay 

/***************SLC 和MLC的参数配置************************/
#define SLC_READ_DELAY        (0.0250)
#define SLC_WRITE_DELAY       (0.200)
#define SLC_ERASE_DELAY       1.5 
#define SLC_GC_READ_DELAY  READ_DELAY    // gc read_delay = read delay    
#define SLC_GC_WRITE_DELAY WRITE_DELAY  // gc write_delay = write delay 

#define MLC_READ_DELAY        (0.060)
#define MLC_WRITE_DELAY       (0.800)
#define MLC_ERASE_DELAY       1.5 
#define MLC_GC_READ_DELAY  READ_DELAY    // gc read_delay = read delay    
#define MLC_GC_WRITE_DELAY WRITE_DELAY  // gc write_delay = write delay

#define OOB_READ_DELAY    0.0
#define OOB_WRITE_DELAY   0.0
#define SLC_OOB_READ_DELAY    0.0
#define SLC_OOB_WRITE_DELAY   0.0
#define MLC_OOB_READ_DELAY    0.0
#define MLC_OOB_WRITE_DELAY   0.0

struct ftl_operation * ftl_op;

#define PAGE_READ     0
#define PAGE_WRITE    1
#define OOB_READ      2
#define OOB_WRITE     3
#define BLOCK_ERASE   4
#define GC_PAGE_READ  5
#define GC_PAGE_WRITE 6
#define SLC_PAGE_READ     7
#define SLC_PAGE_WRITE    8
#define SLC_OOB_READ      9
#define SLC_OOB_WRITE     10
#define SLC_BLOCK_ERASE   11
#define SLC_GC_PAGE_READ  12
#define SLC_GC_PAGE_WRITE 13
#define MLC_PAGE_READ     14
#define MLC_PAGE_WRITE    15
#define MLC_OOB_READ      16
#define MLC_OOB_WRITE     17
#define MLC_BLOCK_ERASE   18
#define MLC_GC_PAGE_READ  19
#define MLC_GC_PAGE_WRITE 20

struct lru{
	int blknum;
	int age;
};

void reset_flash_stat();
void reset_SLC_flash_stat();
void reset_MLC_flash_stat();
double calculate_delay_flash();
double calculate_delay_MLC_flash();
double calculate_delay_SLC_flash();
void initFlash();
void endFlash();
void printWearout();
void send_flash_request(int start_blk_no, int block_cnt, int operation, int mapdir_flag,int flash_flag);
void MLC_find_real_max();
int MLC_find_real_min();
void synchronize_disk_flash();
void find_min_cache();
double callFsim(unsigned int secno, int scount, int operation, int flash_flag);
void find_MC_entries(int *arr, int size);
int not_in_cache(unsigned int pageno);
void find_min_block();
void find_max_block();
int find_free_blk_pos(struct lru *arr, int size);
int search_blk_table(struct lru *arr, int size, int val);
int Isincacheblock(unsigned int number,int num);
int Is_blk_in_second(int number,int num);

int write_count;
int read_count;

extern int write_cnt;
extern int read_cnt;
extern int request_cnt;
extern double write_ratio;
extern double read_ratio;

extern int block_index;
extern int block_entry_num;
extern int blocksize;
extern int block_min;
extern int block_max;

extern int total_request_size;
extern int itemcount;
extern double average_request_size;

int flash_read_num;
int flash_write_num;
int flash_gc_read_num;
int flash_gc_write_num;
int flash_erase_num;
int flash_oob_read_num;
int flash_oob_write_num;

int SLC_flash_read_num;
int SLC_flash_write_num;
int SLC_flash_gc_read_num;
int SLC_flash_gc_write_num;
int SLC_flash_erase_num;
int SLC_flash_oob_read_num;
int SLC_flash_oob_write_num;

int MLC_flash_read_num;
int MLC_flash_write_num;
int MLC_flash_gc_read_num;
int MLC_flash_gc_write_num;
int MLC_flash_erase_num;
int MLC_flash_oob_read_num;
int MLC_flash_oob_write_num;

int map_flash_read_num;
int map_flash_write_num;
int map_flash_gc_read_num;
int map_flash_gc_write_num;
int map_flash_erase_num;
int map_flash_oob_read_num;
int map_flash_oob_write_num;

int ftl_type;

extern int total_util_sect_num; 
extern int total_extra_sect_num;
int global_total_blk_num;

int warm_done; 

int total_er_cnt;
int flag_er_cnt;
int block_er_flag[20000];
int block_dead_flag[20000];
int wear_level_flag[20000];
int unique_blk_num; 
int unique_log_blk_num;
int last_unique_log_blk;

int total_extr_blk_num;
int total_init_blk_num;

int rqst_cnt;
int translation_read_num;
int translation_write_num;
int trace_num;

int MAP_REAL_MAX_ENTRIES;
//int MAP_GHOST_MAX_ENTRIES;
int MAP_SEQ_MAX_ENTRIES; 
int MAP_SECOND_MAX_ENTRIES; 

//int buffer[8192];
int *real_arr;  
int *ghost_arr;
int *seq_arr;
int *second_arr;

//该头结点的初始化和释放在对应的dftl.h文件中进行
Node *ADFTL_Head;
int warm_flag;

int big_request_entry;
int big_request_count;

extern int in_cache_count;

