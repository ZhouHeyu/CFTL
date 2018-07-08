/* 
 * Contributors: Youngjae Kim (youkim@cse.psu.edu)
 *               Aayush Gupta (axg354@cse.psu.edu)
 * 
 * In case if you have any doubts or questions, kindly write to: youkim@cse.psu.edu 
 *
 * This source file implements the DFTL FTL scheme.
 * The detail algorithm for the DFTL can be obtainable from 
 * "DFTL: A Flash Translation Layer Employing Demand-based * Selective Caching of Page-level Address Mapping", ASPLOS, 2009".
 * 
 */

#include <stdlib.h>
#include <string.h>

#include "flash.h"
#include "dftl.h"
#include "ssd_interface.h"
#include "disksim_global.h"

_u32 SLC_opm_gc_cost_benefit();
_u32 MLC_opm_gc_cost_benefit();

struct omap_dir *mapdir;
struct omap_dir *MLC_mapdir;
extern struct SLC_nand_blk_info *head;
extern struct SLC_nand_blk_info *tail;
extern int cache_cmt_hit;
extern int cache_scmt_hit;
extern int cache_slcmt_hit;

blk_t extra_blk_num;
_u32 free_blk_no[2];
_u32 free_SLC_blk_no[2];
_u32 free_MLC_blk_no[2];
_u16 free_page_no[2];
_u16 free_SLC_page_no[2];
_u16 free_MLC_page_no[2];

extern int merge_switch_num;
extern int merge_partial_num;
extern int merge_full_num;

extern int MLC_page_num_for_2nd_map_table;
extern int nand_SLC_page_num;
int stat_gc_called_num;
double total_gc_overhead_time;
extern double delay2;
int map_pg_read=0;
int SLC_to_MLC_counts=0;
void SLC_data_move(int blk){
     int i,valid_flag,valid_sect_num;
     int blkno,bcount;
     double delay3;
     _u32 victim_blkno;
     _u32 copy_lsn[S_SECT_NUM_PER_PAGE];

     for(i=0;i<S_PAGE_NUM_PER_BLK;i++){
         valid_flag=SLC_nand_oob_read(S_SECTOR(blk,i*S_SECT_NUM_PER_PAGE));
         if(valid_flag==1){
            valid_sect_num=SLC_nand_page_read(S_SECTOR(blk,i*S_SECT_NUM_PER_PAGE),copy_lsn,1);
            ASSERT(valid_sect_num==4);
            SLC_opagemap[S_BLK_PAGE_NO_SECT(copy_lsn[0])].ppn=-1;
            blkno=(S_BLK_PAGE_NO_SECT(copy_lsn[0])*4)/8;
            blkno*=8;
            bcount=8;
            delay3=callFsim(blkno,bcount,0,1);
            SLC_to_MLC_counts+=1;
            delay2=delay2+delay3;
         }
       
     }
     victim_blkno=blk; 
     SLC_nand_erase(victim_blkno);    
}
_u32 SLC_opm_gc_cost_benefit()
{
  int max_cb = 0;
  int blk_cb;

  _u32 max_blk = -1, i;

  for (i = 0; i < nand_SLC_blk_num; i++) {
    if( i == free_SLC_blk_no[1]){
      continue;
    }

    blk_cb = SLC_nand_blk[i].ipc;

    
    if (blk_cb > max_cb) {
      max_cb = blk_cb;
      max_blk = i;
    }
  }

  ASSERT(max_blk != -1);
  ASSERT(SLC_nand_blk[max_blk].ipc > 0);
  return max_blk;
}

_u32 MLC_opm_gc_cost_benefit()
{
  int max_cb = 0;
  int blk_cb;

  _u32 max_blk = -1, i;

  for (i = 0; i < nand_MLC_blk_num; i++) {
    if(i == free_MLC_blk_no[0] || i == free_MLC_blk_no[1]){
      continue;
    }

    blk_cb = MLC_nand_blk[i].ipc;

    
    if (blk_cb > max_cb) {
      max_cb = blk_cb;
      max_blk = i;
    }
  }

  ASSERT(max_blk != -1);
  ASSERT(MLC_nand_blk[max_blk].ipc > 0);
  return max_blk;
}

size_t opm_read(sect_t lsn, sect_t size, int mapdir_flag)
{
  int i;
  int lpn = lsn/SECT_NUM_PER_PAGE; // logical page number
  int size_page = size/SECT_NUM_PER_PAGE; // size in page 
  int sect_num;

  sect_t s_lsn;	// starting logical sector number
  sect_t s_psn; // starting physical sector number 

  sect_t lsns[SECT_NUM_PER_PAGE];

  ASSERT(lpn < SLC_opagemap_num);
  ASSERT(lpn + size_page <= SLC_opagemap_num);

  memset (lsns, 0xFF, sizeof (lsns));

  sect_num = (size < SECT_NUM_PER_PAGE) ? size : SECT_NUM_PER_PAGE;

  if(mapdir_flag == 2){
    s_psn = mapdir[lpn].ppn * SECT_NUM_PER_PAGE;
  }
  else s_psn = SLC_opagemap[lpn].ppn * SECT_NUM_PER_PAGE;

  s_lsn = lpn * SECT_NUM_PER_PAGE;

  for (i = 0; i < SECT_NUM_PER_PAGE; i++) {
    lsns[i] = s_lsn + i;
  }

  if(mapdir_flag == 2){
    map_pg_read++;
  }
  size = nand_page_read(s_psn, lsns, 0);

  ASSERT(size == SECT_NUM_PER_PAGE);

  return sect_num;
}
size_t SLC_opm_read(sect_t lsn, sect_t size, int mapdir_flag)
{
  int i;
  int lpn = lsn/S_SECT_NUM_PER_PAGE; // logical page number
  int size_page = size/S_SECT_NUM_PER_PAGE; // size in page 
  int sect_num;

  sect_t s_lsn;	// starting logical sector number
  sect_t s_psn; // starting physical sector number 

  sect_t lsns[S_SECT_NUM_PER_PAGE];

  ASSERT(lpn < SLC_opagemap_num);
  ASSERT(lpn + size_page <= SLC_opagemap_num);

  memset (lsns, 0xFF, sizeof (lsns));

  sect_num = (size < S_SECT_NUM_PER_PAGE) ? size : S_SECT_NUM_PER_PAGE;

  if(mapdir_flag == 2){
    s_psn = mapdir[lpn].ppn * S_SECT_NUM_PER_PAGE;
  }
  else s_psn = SLC_opagemap[lpn].ppn * S_SECT_NUM_PER_PAGE;

  s_lsn = lpn * S_SECT_NUM_PER_PAGE;

  for (i = 0; i < S_SECT_NUM_PER_PAGE; i++) {
    lsns[i] = s_lsn + i;
  }

  if(mapdir_flag == 2){
    map_pg_read++;
  }
  size = SLC_nand_page_read(s_psn, lsns, 0);

  ASSERT(size == S_SECT_NUM_PER_PAGE);

  return sect_num;
}
size_t MLC_opm_read(sect_t lsn, sect_t size, int mapdir_flag)
{
  int i;
  int lpn = lsn/M_SECT_NUM_PER_PAGE; // logical page number
  int size_page = size/M_SECT_NUM_PER_PAGE; // size in page 
  int sect_num;

  sect_t s_lsn;	// starting logical sector number
  sect_t s_psn; // starting physical sector number 

  sect_t lsns[M_SECT_NUM_PER_PAGE];

  ASSERT(lpn < MLC_opagemap_num);
  ASSERT(lpn + size_page <= MLC_opagemap_num);

  memset (lsns, 0xFF, sizeof (lsns));

  sect_num = (size < M_SECT_NUM_PER_PAGE) ? size : M_SECT_NUM_PER_PAGE;

  if(mapdir_flag == 2){
    s_psn = MLC_mapdir[lpn].ppn * M_SECT_NUM_PER_PAGE;
  }
  else s_psn = MLC_opagemap[lpn].ppn * M_SECT_NUM_PER_PAGE;

  s_lsn = lpn * M_SECT_NUM_PER_PAGE;

  for (i = 0; i < M_SECT_NUM_PER_PAGE; i++) {
    lsns[i] = s_lsn + i;
  }

  if(mapdir_flag == 2){
    map_pg_read++;
  }
  size = MLC_nand_page_read(s_psn, lsns, 0);

  ASSERT(size == M_SECT_NUM_PER_PAGE);

  return sect_num;
}

int SLC_opm_gc_get_free_blk(int small, int mapdir_flag)
{
  if (free_SLC_page_no[small] >= S_SECT_NUM_PER_BLK) {

    free_SLC_blk_no[small] = nand_get_SLC_free_blk(1);

    free_SLC_page_no[small] = 0;

    return -1;
  }
  
  return 0;
}
int MLC_opm_gc_get_free_blk(int small, int mapdir_flag)
{
  if (free_MLC_page_no[small] >= M_SECT_NUM_PER_BLK) {

    free_MLC_blk_no[small] = nand_get_MLC_free_blk(1);

    free_MLC_page_no[small] = 0;

    return -1;
  }
  
  return 0;
}
int SLC_opm_gc_run(int small, int mapdir_flag)
{
  blk_t victim_blk_no;
  int merge_count;
  int i,z, j,m,q, benefit = 0;
  int k,old_flag,temp_arr[S_PAGE_NUM_PER_BLK],temp_arr1[S_PAGE_NUM_PER_BLK],map_arr[S_PAGE_NUM_PER_BLK]; 
  int valid_flag,pos;

  _u32 copy_lsn[S_SECT_NUM_PER_PAGE], copy[S_SECT_NUM_PER_PAGE];
  _u16 valid_sect_num,  l, s;

  victim_blk_no = SLC_opm_gc_cost_benefit();//选出无效页最多的块
  memset(copy_lsn, 0xFF, sizeof (copy_lsn));

  s = k = S_OFF_F_SECT(free_SLC_page_no[small]);

  if(!((s == 0) && (k == 0))){
    printf("s && k should be 0\n");
    exit(0);
  }
 
  small = -1;

  for( q = 0; q < S_PAGE_NUM_PER_BLK; q++){
    if(SLC_nand_blk[victim_blk_no].page_status[q] == 1){ //map block
      for( q = 0; q  < 64; q++) {
        if(SLC_nand_blk[victim_blk_no].page_status[q] == 0 ){
          printf("something corrupted1=%d",victim_blk_no);
        }
      }
      small = 0;
      break;
    } 
    else if(SLC_nand_blk[victim_blk_no].page_status[q] == 0){ //data block
      for( q = 0; q  < 64; q++) {
        if(SLC_nand_blk[victim_blk_no].page_status[q] == 1 ){
          printf("something corrupted2",victim_blk_no);
        }
      }
      small = 1;
      break;
    }
  }

  ASSERT ( small == 0 || small == 1);
  pos = 0;
  merge_count = 0;
  for (i = 0; i < S_PAGE_NUM_PER_BLK; i++) 
  {

    valid_flag = SLC_nand_oob_read( S_SECTOR(victim_blk_no, i * S_SECT_NUM_PER_PAGE));

    if(valid_flag == 1)//非空闲且有效
    {
        valid_sect_num = SLC_nand_page_read( S_SECTOR(victim_blk_no, i * S_SECT_NUM_PER_PAGE), copy, 1);

        merge_count++;

        ASSERT(valid_sect_num == 4);
        k=0;
        for (j = 0; j < valid_sect_num; j++) {
          copy_lsn[k] = copy[j];
          k++;
        }

          benefit += SLC_opm_gc_get_free_blk(small, mapdir_flag);

          if(SLC_nand_blk[victim_blk_no].page_status[i] == 1)//剔除翻译块
          {                       
            mapdir[(copy_lsn[s]/S_SECT_NUM_PER_PAGE)].ppn  = S_BLK_PAGE_NO_SECT(S_SECTOR(free_SLC_blk_no[small], free_SLC_page_no[small]));
            SLC_opagemap[copy_lsn[s]/S_SECT_NUM_PER_PAGE].ppn = S_BLK_PAGE_NO_SECT(S_SECTOR(free_SLC_blk_no[small], free_SLC_page_no[small]));

            SLC_nand_page_write(S_SECTOR(free_SLC_blk_no[small],free_SLC_page_no[small]) & (~S_OFF_MASK_SECT), copy_lsn, 1, 2);
            free_SLC_page_no[small] += S_SECT_NUM_PER_PAGE;
          }
          else{//剔除的是数据块


            SLC_opagemap[S_BLK_PAGE_NO_SECT(copy_lsn[s])].ppn = S_BLK_PAGE_NO_SECT(S_SECTOR(free_SLC_blk_no[small], free_SLC_page_no[small]));

            SLC_nand_page_write(S_SECTOR(free_SLC_blk_no[small],free_SLC_page_no[small]) & (~S_OFF_MASK_SECT), copy_lsn, 1, 1);
            free_SLC_page_no[small] += S_SECT_NUM_PER_PAGE;

            if((SLC_opagemap[S_BLK_PAGE_NO_SECT(copy_lsn[s])].map_status == MAP_REAL) || (SLC_opagemap[S_BLK_PAGE_NO_SECT(copy_lsn[s])].map_status == MAP_GHOST)) {
              delay_flash_update++;
            }
        
            else {
  
              map_arr[pos] = copy_lsn[s];
              pos++;
            } 
          }
    }
  }
  
  for(i=0;i < S_PAGE_NUM_PER_BLK;i++) {
      temp_arr[i]=-1;
  }
  k=0;
  for(i =0 ; i < pos; i++) {
      old_flag = 0;
      for( j = 0 ; j < k; j++) {
           if(temp_arr[j] == mapdir[((map_arr[i]/S_SECT_NUM_PER_PAGE)/SLC_MAP_ENTRIES_PER_PAGE)].ppn) {
                if(temp_arr[j] == -1){
                      printf("something wrong");
                      ASSERT(0);
                }
                old_flag = 1;
                break;
           }
      }
      if( old_flag == 0 ) {
           temp_arr[k] = mapdir[((map_arr[i]/S_SECT_NUM_PER_PAGE)/SLC_MAP_ENTRIES_PER_PAGE)].ppn;
           temp_arr1[k] = map_arr[i];
           k++;
      }
      else
        save_count++;
  }

  for ( i=0; i < k; i++) {
            if (free_SLC_page_no[0] >= S_SECT_NUM_PER_BLK) {
                if((free_SLC_blk_no[0] = nand_get_SLC_free_blk(1)) == -1){
                   printf("we are in big trouble shudnt happen");
                }

                free_SLC_page_no[0] = 0;
            }
     
            SLC_nand_page_read(temp_arr[i]*S_SECT_NUM_PER_PAGE,copy,1);

            for(m = 0; m<S_SECT_NUM_PER_PAGE; m++){
               SLC_nand_invalidate(mapdir[((temp_arr1[i]/S_SECT_NUM_PER_PAGE)/SLC_MAP_ENTRIES_PER_PAGE)].ppn*S_SECT_NUM_PER_PAGE+m, copy[m]);
              } 
            nand_stat(SLC_OOB_WRITE);


            mapdir[((temp_arr1[i]/S_SECT_NUM_PER_PAGE)/SLC_MAP_ENTRIES_PER_PAGE)].ppn  = S_BLK_PAGE_NO_SECT(S_SECTOR(free_SLC_blk_no[0], free_SLC_page_no[0]));
            SLC_opagemap[((temp_arr1[i]/S_SECT_NUM_PER_PAGE)/SLC_MAP_ENTRIES_PER_PAGE)].ppn = S_BLK_PAGE_NO_SECT(S_SECTOR(free_SLC_blk_no[0], free_SLC_page_no[0]));

            SLC_nand_page_write(S_SECTOR(free_SLC_blk_no[0],free_SLC_page_no[0]) & (~S_OFF_MASK_SECT), copy, 1, 2);
      
            free_SLC_page_no[0] += S_SECT_NUM_PER_PAGE;


  }
  if(merge_count == 0 ) 
    merge_switch_num++;
  else if(merge_count > 0 && merge_count < S_PAGE_NUM_PER_BLK)
    merge_partial_num++;
  else if(merge_count == S_PAGE_NUM_PER_BLK)
    merge_full_num++;
  else if(merge_count > S_PAGE_NUM_PER_BLK){
    printf("merge_count =%d PAGE_NUM_PER_BLK=%d",merge_count,S_PAGE_NUM_PER_BLK);
    ASSERT(0);
  }

  SLC_nand_erase(victim_blk_no);

  return (benefit + 1);
}
int MLC_opm_gc_run(int small, int mapdir_flag)
{
  blk_t victim_blk_no;
  int merge_count;
  int i,z, j,m,q, benefit = 0;
  int k,old_flag,temp_arr[M_PAGE_NUM_PER_BLK],temp_arr1[M_PAGE_NUM_PER_BLK],map_arr[M_PAGE_NUM_PER_BLK]; 
  int valid_flag,pos;

  _u32 copy_lsn[M_SECT_NUM_PER_PAGE], copy[M_SECT_NUM_PER_PAGE];
  _u16 valid_sect_num,  l, s;

  victim_blk_no = MLC_opm_gc_cost_benefit();
  memset(copy_lsn, 0xFF, sizeof (copy_lsn));

  s = k = M_OFF_F_SECT(free_MLC_page_no[small]);

  if(!((s == 0) && (k == 0))){
    printf("s && k should be 0\n");
    exit(0);
  }
 
  small = -1;

  for( q = 0; q < M_PAGE_NUM_PER_BLK; q++){
    if(MLC_nand_blk[victim_blk_no].page_status[q] == 1){ //map block
      for( q = 0; q  < 128; q++) {
        if(MLC_nand_blk[victim_blk_no].page_status[q] == 0 ){
          printf("something corrupted1=%d",victim_blk_no);
        }
      }
      small = 0;
      break;
    } 
    else if(MLC_nand_blk[victim_blk_no].page_status[q] == 0){ //data block
      for( q = 0; q  < 128; q++) {
        if(MLC_nand_blk[victim_blk_no].page_status[q] == 1 ){
          printf("something corrupted2",victim_blk_no);
        }
      }
      small = 1;
      break;
    }
  }

  ASSERT ( small == 0 || small == 1);
  pos = 0;
  merge_count = 0;
  for (i = 0; i < M_PAGE_NUM_PER_BLK; i++) 
  {

    valid_flag = MLC_nand_oob_read( M_SECTOR(victim_blk_no, i * M_SECT_NUM_PER_PAGE));

    if(valid_flag == 1)
    {
        valid_sect_num = MLC_nand_page_read( M_SECTOR(victim_blk_no, i * M_SECT_NUM_PER_PAGE), copy, 1);

        merge_count++;

        ASSERT(valid_sect_num == 8);
        k=0;
        for (j = 0; j < valid_sect_num; j++) {
          copy_lsn[k] = copy[j];
          k++;
        }

          benefit += MLC_opm_gc_get_free_blk(small, mapdir_flag);

          if(MLC_nand_blk[victim_blk_no].page_status[i] == 1)
          {                       
            MLC_mapdir[(copy_lsn[s]/M_SECT_NUM_PER_PAGE)].ppn  = M_BLK_PAGE_NO_SECT(M_SECTOR(free_MLC_blk_no[small], free_MLC_page_no[small]));
            MLC_opagemap[copy_lsn[s]/M_SECT_NUM_PER_PAGE].ppn = M_BLK_PAGE_NO_SECT(M_SECTOR(free_MLC_blk_no[small], free_MLC_page_no[small]));

            MLC_nand_page_write(M_SECTOR(free_MLC_blk_no[small],free_MLC_page_no[small]) & (~M_OFF_MASK_SECT), copy_lsn, 1, 2);
            free_MLC_page_no[small] += M_SECT_NUM_PER_PAGE;
          }
          else{


            MLC_opagemap[M_BLK_PAGE_NO_SECT(copy_lsn[s])].ppn = M_BLK_PAGE_NO_SECT(M_SECTOR(free_MLC_blk_no[small], free_MLC_page_no[small]));

            MLC_nand_page_write(M_SECTOR(free_MLC_blk_no[small],free_MLC_page_no[small]) & (~M_OFF_MASK_SECT), copy_lsn, 1, 1);
            free_MLC_page_no[small] += M_SECT_NUM_PER_PAGE;

            if((MLC_opagemap[M_BLK_PAGE_NO_SECT(copy_lsn[s])].map_status == MAP_REAL) || (MLC_opagemap[M_BLK_PAGE_NO_SECT(copy_lsn[s])].map_status == MAP_GHOST)) {
              delay_flash_update++;
            }
        
            else {
  
              map_arr[pos] = copy_lsn[s];
              pos++;
            } 
          }
    }
  }
  
  for(i=0;i < M_PAGE_NUM_PER_BLK;i++) {
      temp_arr[i]=-1;
  }
  k=0;
  for(i =0 ; i < pos; i++) {
      old_flag = 0;
      for( j = 0 ; j < k; j++) {
           if(temp_arr[j] == MLC_mapdir[((map_arr[i]/M_SECT_NUM_PER_PAGE)/MLC_MAP_ENTRIES_PER_PAGE)].ppn) {
                if(temp_arr[j] == -1){
                      printf("something wrong");
                      ASSERT(0);
                }
                old_flag = 1;
                break;
           }
      }
      if( old_flag == 0 ) {
           temp_arr[k] = MLC_mapdir[((map_arr[i]/M_SECT_NUM_PER_PAGE)/MLC_MAP_ENTRIES_PER_PAGE)].ppn;
           temp_arr1[k] = map_arr[i];
           k++;
      }
      else
        save_count++;
  }

  for ( i=0; i < k; i++) {
            if (free_MLC_page_no[0] >= M_SECT_NUM_PER_BLK) {
                if((free_MLC_blk_no[0] = nand_get_MLC_free_blk(1)) == -1){
                   printf("we are in big trouble shudnt happen");
                }

                free_MLC_page_no[0] = 0;
            }
     
            MLC_nand_page_read(temp_arr[i]*M_SECT_NUM_PER_PAGE,copy,1);

            for(m = 0; m<M_SECT_NUM_PER_PAGE; m++){
               MLC_nand_invalidate(MLC_mapdir[((temp_arr1[i]/M_SECT_NUM_PER_PAGE)/MLC_MAP_ENTRIES_PER_PAGE)].ppn*M_SECT_NUM_PER_PAGE+m, copy[m]);
              } 
            nand_stat(MLC_OOB_WRITE);


            MLC_mapdir[((temp_arr1[i]/M_SECT_NUM_PER_PAGE)/MLC_MAP_ENTRIES_PER_PAGE)].ppn  = M_BLK_PAGE_NO_SECT(M_SECTOR(free_MLC_blk_no[0], free_MLC_page_no[0]));
            MLC_opagemap[((temp_arr1[i]/M_SECT_NUM_PER_PAGE)/MLC_MAP_ENTRIES_PER_PAGE)].ppn = M_BLK_PAGE_NO_SECT(M_SECTOR(free_MLC_blk_no[0], free_MLC_page_no[0]));

            MLC_nand_page_write(M_SECTOR(free_MLC_blk_no[0],free_MLC_page_no[0]) & (~M_OFF_MASK_SECT), copy, 1, 2);
      
            free_MLC_page_no[0] += M_SECT_NUM_PER_PAGE;


  }
  if(merge_count == 0 ) 
    merge_switch_num++;
  else if(merge_count > 0 && merge_count < M_PAGE_NUM_PER_BLK)
    merge_partial_num++;
  else if(merge_count == M_PAGE_NUM_PER_BLK)
    merge_full_num++;
  else if(merge_count > M_PAGE_NUM_PER_BLK){
    printf("merge_count =%d PAGE_NUM_PER_BLK=%d",merge_count,M_PAGE_NUM_PER_BLK);
    ASSERT(0);
  }

  MLC_nand_erase(victim_blk_no);

  return (benefit + 1);
}

size_t SLC_opm_write(sect_t lsn, sect_t size, int mapdir_flag)  
{
  int i,a,b,c,d,e,valid_flag,valid_sect_num;
  int lpn = lsn/S_SECT_NUM_PER_PAGE; // logical page number
  int size_page = size/S_SECT_NUM_PER_PAGE; // size in page 
  int ppn;
  int small;
  int blkno,bcount;
  

  sect_t lsns[S_SECT_NUM_PER_PAGE];
  int sect_num = S_SECT_NUM_PER_PAGE;

  sect_t s_lsn;	// starting logical sector number
  sect_t s_psn; // starting physical sector number 
  sect_t s_psn1;


  ASSERT(lpn < SLC_opagemap_num);
  ASSERT(lpn + size_page <= SLC_opagemap_num);

  s_lsn = lpn * S_SECT_NUM_PER_PAGE;


  if(mapdir_flag == 2) //map page
    small = 0;
  else if ( mapdir_flag == 1) //data page
    small = 1;
  else{
    printf("something corrupted");
    exit(0);
  }

  if (free_SLC_page_no[small] >= S_SECT_NUM_PER_BLK) 
    {
      free_SLC_page_no[small] = 0;  
      if((head-SLC_nand_blk)<4090&&(head-SLC_nand_blk)>=(tail-SLC_nand_blk))//3代表4096  5 代表8192
      {  head++;
        // printf("进入02后的状态：%d\n",(head-tail));
         if((head-SLC_nand_blk)==4090){
            b=tail-SLC_nand_blk;
            SLC_data_move(b);//SLC_data_move(b);
            tail++;
            printf("第2种情况：\n");
            printf("头指针＝%d\n",(head-SLC_nand_blk));
            printf("尾指针＝%d\n",(tail-SLC_nand_blk));
         }
         printf("第1种情况：\n");
         printf("头指针＝%d\n",(head-SLC_nand_blk));
         printf("尾指针＝%d\n",(tail-SLC_nand_blk));
      }else if((head-SLC_nand_blk)<(tail-SLC_nand_blk)&&(head-SLC_nand_blk)<4090){
         b=tail-SLC_nand_blk;
         SLC_data_move(b);//SLC_data_move(b);
         head++;        
         tail++;
         printf("第0种情况：\n");
         printf("头指针＝%d\n",(head-SLC_nand_blk));
         printf("尾指针＝%d\n",(tail-SLC_nand_blk));
         if((tail-SLC_nand_blk)==4096){
             tail=&SLC_nand_blk[0];
             printf("第7种情况：\n");
             printf("头指针＝%d\n",(head-SLC_nand_blk));
             printf("尾指针＝%d\n",(tail-SLC_nand_blk));
              }         
      }else if((head-SLC_nand_blk)>=4090&&(head-SLC_nand_blk)<4096&&(tail-SLC_nand_blk)<=4095){
         b=tail-SLC_nand_blk;
         SLC_data_move(b);//SLC_data_move(b);
         head++;        
         tail++;
         if((head-SLC_nand_blk)==4096){
            head=&SLC_nand_blk[0];
            printf("第4种情况：\n");
            printf("头指针＝%d\n",(head-SLC_nand_blk));
            printf("尾指针＝%d\n",(tail-SLC_nand_blk));
         }
        printf("第3种情况：\n");
        printf("头指针＝%d\n",(head-SLC_nand_blk));
        printf("尾指针＝%d\n",(tail-SLC_nand_blk)); 
       }else {
         b=tail-SLC_nand_blk;
         SLC_data_move(b);//SLC_data_move(b);
         head++;
         tail++;
         if((tail-SLC_nand_blk)==4096){
            tail=&SLC_nand_blk[0];
            printf("第6种情况：\n");
            printf("头指针＝%d\n",(head-SLC_nand_blk));
            printf("尾指针＝%d\n",(tail-SLC_nand_blk));
         }
         printf("第5种情况：\n");
         printf("头指针＝%d\n",(head-SLC_nand_blk));
         printf("尾指针＝%d\n",(tail-SLC_nand_blk));         
      }                    
      free_SLC_blk_no[small]=nand_get_SLC_free_blk(0);
    /*  printf("%d\n",free_SLC_blk_no[small]);
      printf("\n");*/
  }
 // printf("头指针＝%d\n",(head-SLC_nand_blk));
 // printf("尾指针＝%d\n",(tail-SLC_nand_blk));
  memset (lsns, 0xFF, sizeof (lsns));
  
  s_psn = S_SECTOR(free_SLC_blk_no[small], free_SLC_page_no[small]);

  if(s_psn % 4 != 0){
    printf("s_psn: %d\n", s_psn);
  }

  ppn = s_psn / S_SECT_NUM_PER_PAGE;
  if(SLC_opagemap[lpn].ppn!=-1){
  if (SLC_opagemap[lpn].free == 0) {
    s_psn1 = SLC_opagemap[lpn].ppn * S_SECT_NUM_PER_PAGE;
    for(i = 0; i<S_SECT_NUM_PER_PAGE; i++){
      SLC_nand_invalidate(s_psn1 + i, s_lsn + i);
    } 
    nand_stat(10);
  }
  else {
    SLC_opagemap[lpn].free = 0;
  }
 }
  for (i = 0; i < S_SECT_NUM_PER_PAGE; i++) 
  {
    lsns[i] = s_lsn + i;
  }

  if(mapdir_flag == 2) {
    mapdir[lpn].ppn = ppn;
    SLC_opagemap[lpn].ppn = ppn;
  }
  else {
    SLC_opagemap[lpn].ppn = ppn;
  }

  free_SLC_page_no[small] += S_SECT_NUM_PER_PAGE;

  SLC_nand_page_write(s_psn, lsns, 0, mapdir_flag);

  return sect_num;
}
size_t MLC_opm_write(sect_t lsn, sect_t size, int mapdir_flag)  
{
  int i;
  int lpn = lsn/M_SECT_NUM_PER_PAGE; // logical page number
  int size_page = size/M_SECT_NUM_PER_PAGE; // size in page 
  int ppn;
  int small;

  sect_t lsns[M_SECT_NUM_PER_PAGE];
  int sect_num = M_SECT_NUM_PER_PAGE;

  sect_t s_lsn;	// starting logical sector number
  sect_t s_psn; // starting physical sector number 
  sect_t s_psn1;


  ASSERT(lpn < MLC_opagemap_num);
  ASSERT(lpn + size_page <= MLC_opagemap_num);

  s_lsn = lpn * M_SECT_NUM_PER_PAGE;


  if(mapdir_flag == 2) //map page
    small = 0;
  else if ( mapdir_flag == 1) //data page
    small = 1;
  else{
    printf("something corrupted");
    exit(0);
  }

  if (free_MLC_page_no[small] >= M_SECT_NUM_PER_BLK) 
  {

    if ((free_MLC_blk_no[small] = nand_get_MLC_free_blk(0)) == -1) 
    {
      int j = 0;

      while (free_MLC_blk_num < 3 ){
        j += MLC_opm_gc_run(small, mapdir_flag);
      }
      MLC_opm_gc_get_free_blk(small, mapdir_flag);
    } 
    else {
      free_MLC_page_no[small] = 0;
    }
  }
  
  memset (lsns, 0xFF, sizeof (lsns));
  
  s_psn = M_SECTOR(free_MLC_blk_no[small], free_MLC_page_no[small]);

  if(s_psn % 8 != 0){
    printf("s_psn: %d\n", s_psn);
  }

  ppn = s_psn / M_SECT_NUM_PER_PAGE;

  if (MLC_opagemap[lpn].free == 0) {
    s_psn1 = MLC_opagemap[lpn].ppn * M_SECT_NUM_PER_PAGE;
    for(i = 0; i<M_SECT_NUM_PER_PAGE; i++){
      MLC_nand_invalidate(s_psn1 + i, s_lsn + i);
    } 
    nand_stat(17);
  }
  else {
    MLC_opagemap[lpn].free = 0;
  }

  for (i = 0; i < M_SECT_NUM_PER_PAGE; i++) 
  {
    lsns[i] = s_lsn + i;
  }

  if(mapdir_flag == 2) {
    MLC_mapdir[lpn].ppn = ppn;
    MLC_opagemap[lpn].ppn = ppn;
  }
  else {
    MLC_opagemap[lpn].ppn = ppn;
  }

  free_MLC_page_no[small] += M_SECT_NUM_PER_PAGE;

  MLC_nand_page_write(s_psn, lsns, 0, mapdir_flag);

  return sect_num;
}
void opm_end()
{
  if (SLC_opagemap != NULL) {
    free(SLC_opagemap);
    free(mapdir);
  }
  
  SLC_opagemap_num = 0;
  if(ADFTL_Head!=NULL){
      FreeList(ADFTL_Head);
  }

}

void opagemap_reset()
{
  cache_cmt_hit = 0;
  cache_scmt_hit = 0;
  cache_slcmt_hit = 0;
  flash_hit = 0;
  disk_hit = 0;
  evict = 0;
  delay_flash_update = 0; 
  read_count =0;
  write_count=0;
  map_pg_read=0;
  save_count = 0;
}

int opm_init(blk_t SLC_blk_num,blk_t MLC_blk_num, blk_t extra_num )
{
  int i;
  int mapdir_num,SLC_mapdir_num,MLC_mapdir_num;

  SLC_opagemap_num = SLC_blk_num * S_PAGE_NUM_PER_BLK;
  MLC_opagemap_num = MLC_blk_num * M_PAGE_NUM_PER_BLK;
  opagemap_num = SLC_opagemap_num + MLC_opagemap_num;



  //create primary mapping table
  SLC_opagemap = (struct opm_entry *) malloc(sizeof (struct opm_entry) * SLC_opagemap_num);
  MLC_opagemap = (struct opm_entry *) malloc(sizeof (struct opm_entry) * MLC_opagemap_num);
  SLC_mapdir_num = SLC_opagemap_num / SLC_MAP_ENTRIES_PER_PAGE;
  MLC_mapdir_num = MLC_opagemap_num / MLC_MAP_ENTRIES_PER_PAGE;
  mapdir_num = SLC_mapdir_num + MLC_mapdir_num;
  mapdir = (struct omap_dir *)malloc(sizeof(struct omap_dir) * SLC_mapdir_num ); 
  MLC_mapdir = (struct omap_dir *)malloc(sizeof(struct omap_dir) * MLC_mapdir_num );
  
  if ((SLC_opagemap == NULL) ||(MLC_opagemap == NULL) || (mapdir == NULL)|| (MLC_mapdir == NULL)) {
    return -1;
  }
  
  if(((SLC_opagemap_num % SLC_MAP_ENTRIES_PER_PAGE) != 0)&&((MLC_opagemap_num % MLC_MAP_ENTRIES_PER_PAGE) != 0)){
    printf("opagemap_num % MAP_ENTRIES_PER_PAGE is not zero\n"); 
    mapdir_num++;
  }

  memset(SLC_opagemap, 0xFF, sizeof (struct opm_entry) * SLC_opagemap_num);
  memset(MLC_opagemap, 0xFF, sizeof (struct opm_entry) * MLC_opagemap_num);
  memset(mapdir,  0xFF, sizeof (struct omap_dir) * SLC_mapdir_num);
  memset(MLC_mapdir,  0xFF, sizeof (struct omap_dir) * MLC_mapdir_num);
  //youkim: 1st map table 
  TOTAL_MAP_ENTRIES = opagemap_num;
  SLC_TOTAL_MAP_ENTRIES = SLC_opagemap_num;
  MLC_TOTAL_MAP_ENTRIES = MLC_opagemap_num;

  for(i = 0; i<SLC_TOTAL_MAP_ENTRIES; i++){
    SLC_opagemap[i].cache_status = 0;
    SLC_opagemap[i].cache_age = 0;
    SLC_opagemap[i].map_status = 0;
    SLC_opagemap[i].map_age = 0;
    SLC_opagemap[i].update = 0;
  }
  for(i = 0; i<MLC_TOTAL_MAP_ENTRIES; i++){
    MLC_opagemap[i].cache_status = 0;
    MLC_opagemap[i].cache_age = 0;
    MLC_opagemap[i].map_status = 0;
    MLC_opagemap[i].map_age = 0;
    MLC_opagemap[i].update = 0;
  }
  extra_blk_num = extra_num;

  free_SLC_blk_no[1] = nand_get_SLC_free_blk(0);//wfk:获取第0个块作为SLC的翻译块
  free_SLC_page_no[1] = 0;
  free_MLC_blk_no[0] = nand_get_MLC_free_blk(0);//wfk:获取第4096个块作为MLC翻译块
  free_MLC_page_no[0] = 0;
  free_MLC_blk_no[1] = nand_get_MLC_free_blk(0);//wfk:获取第4097个块作为MLC的数据块
  free_MLC_page_no[1] = 0;

  //initialize variables
  MAP_REAL_NUM_ENTRIES = 0;
  MAP_SEQ_NUM_ENTRIES = 0;
  CACHE_NUM_ENTRIES = 0;
  SYNC_NUM = 0;
  
  cache_cmt_hit = 0;
  cache_scmt_hit = 0;
  cache_slcmt_hit = 0;
  cache_hit = 0;
  flash_hit = 0;
  disk_hit = 0;
  evict = 0;
  read_cache_hit = 0;
  write_cache_hit = 0;
  write_count =0;
  read_count = 0;
  save_count = 0;

  //update 2nd mapping table
  
  for(i = 0; i<MLC_mapdir_num; i++){
    ASSERT(MLC_MAP_ENTRIES_PER_PAGE == 1024);
    MLC_opm_write(i*M_SECT_NUM_PER_PAGE, M_SECT_NUM_PER_PAGE, 2);
  }

  /*
  for(i = mapdir_num; i<(opagemap_num - mapdir_num - (extra_num * PAGE_NUM_PER_BLK)); i++){
    opm_write(i*SECT_NUM_PER_PAGE, SECT_NUM_PER_PAGE, 1);
  }
  */

  // update dm_table
  /*
  int j;
  for(i = mapdir_num; i<(opagemap_num - mapdir_num - (extra_num * PAGE_NUM_PER_BLK)); i++){
      for(j=0; j < SECT_NUM_PER_PAGE;j++)
        dm_table[ (i*SECT_NUM_PER_PAGE) + j] = DEV_FLASH;
  }
  */
  
  return 0;
}
/*
int SLC_opm_invalid(int secno)
{
  int lpn = secno/S_SECT_NUM_PER_PAGE + page_num_for_2nd_map_table;	
  int s_lsn = lpn * S_SECT_NUM_PER_PAGE;
  int i, s_psn1;

  s_psn1 = SLC_opagemap[lpn].ppn * S_SECT_NUM_PER_PAGE;
  for(i = 0; i<S_SECT_NUM_PER_PAGE; i++){
      SLC_nand_invalidate(s_psn1 + i, s_lsn + i);
  }
  SLC_opagemap[lpn].ppn = -1;
  SLC_opagemap[lpn].map_status = 0;
  SLC_opagemap[lpn].map_age = 0;
  SLC_opagemap[lpn].update = 0;

  return SECT_NUM_PER_PAGE;

}*/
int MLC_opm_invalid(int secno)
{
  int lpn = secno/M_SECT_NUM_PER_PAGE + MLC_page_num_for_2nd_map_table;	
  int s_lsn = lpn * M_SECT_NUM_PER_PAGE;
  int i, s_psn1;

  s_psn1 = MLC_opagemap[lpn].ppn * M_SECT_NUM_PER_PAGE;
  for(i = 0; i<M_SECT_NUM_PER_PAGE; i++){
      MLC_nand_invalidate(s_psn1 + i, s_lsn + i);
  }
  MLC_opagemap[lpn].ppn = -1;
  MLC_opagemap[lpn].map_status = 0;
  MLC_opagemap[lpn].map_age = 0;
  MLC_opagemap[lpn].update = 0;

  return SECT_NUM_PER_PAGE;

}

struct ftl_operation opm_operation = {
  init:  opm_init,
  Sopm_read:  SLC_opm_read,
  Mopm_read:  MLC_opm_read, 
  Sopm_write: SLC_opm_write,
  Mopm_write: MLC_opm_write,
  end:   opm_end
};
  
struct ftl_operation * opm_setup()
{
  return &opm_operation;
}
