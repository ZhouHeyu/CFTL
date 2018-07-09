/*
 * DiskSim Storage Subsystem Simulation Environment (Version 3.0)
 * Revision Authors: John Bucy, Greg Ganger
 * Contributors: John Griffin, Jiri Schindler, Steve Schlosser
 *
 * Copyright (c) of Carnegie Mellon University, 2001, 2002, 2003.
 *
 * This software is being provided by the copyright holders under the
 * following license. By obtaining, using and/or copying this software,
 * you agree that you have read, understood, and will comply with the
 * following terms and conditions:
 *
 * Permission to reproduce, use, and prepare derivative works of this
 * software is granted provided the copyright and "No Warranty" statements
 * are included with all reproductions and derivative works and associated
 * documentation. This software may also be redistributed without charge
 * provided that the copyright and "No Warranty" statements are included
 * in all redistributions.
 *
 * NO WARRANTY. THIS SOFTWARE IS FURNISHED ON AN "AS IS" BASIS.
 * CARNEGIE MELLON UNIVERSITY MAKES NO WARRANTIES OF ANY KIND, EITHER
 * EXPRESSED OR IMPLIED AS TO THE MATTER INCLUDING, BUT NOT LIMITED
 * TO: WARRANTY OF FITNESS FOR PURPOSE OR MERCHANTABILITY, EXCLUSIVITY
 * OF RESULTS OR RESULTS OBTAINED FROM USE OF THIS SOFTWARE. CARNEGIE
 * MELLON UNIVERSITY DOES NOT MAKE ANY WARRANTY OF ANY KIND WITH RESPECT
 * TO FREEDOM FROM PATENT, TRADEMARK, OR COPYRIGHT INFRINGEMENT.
 * COPYRIGHT HOLDERS WILL BEAR NO LIABILITY FOR ANY USE OF THIS SOFTWARE
 * OR DOCUMENTATION.
 *
 */



/*
 * DiskSim Storage Subsystem Simulation Environment (Version 2.0)
 * Revision Authors: Greg Ganger
 * Contributors: Ross Cohen, John Griffin, Steve Schlosser
 *
 * Copyright (c) of Carnegie Mellon University, 1999.
 *
 * Permission to reproduce, use, and prepare derivative works of
 * this software for internal use is granted provided the copyright
 * and "No Warranty" statements are included with all reproductions
 * and derivative works. This software may also be redistributed
 * without charge provided that the copyright and "No Warranty"
 * statements are included in all redistributions.
 *
 * NO WARRANTY. THIS SOFTWARE IS FURNISHED ON AN "AS IS" BASIS.
 * CARNEGIE MELLON UNIVERSITY MAKES NO WARRANTIES OF ANY KIND, EITHER
 * EXPRESSED OR IMPLIED AS TO THE MATTER INCLUDING, BUT NOT LIMITED
 * TO: WARRANTY OF FITNESS FOR PURPOSE OR MERCHANTABILITY, EXCLUSIVITY
 * OF RESULTS OR RESULTS OBTAINED FROM USE OF THIS SOFTWARE. CARNEGIE
 * MELLON UNIVERSITY DOES NOT MAKE ANY WARRANTY OF ANY KIND WITH RESPECT
 * TO FREEDOM FROM PATENT, TRADEMARK, OR COPYRIGHT INFRINGEMENT.
 */


#include "disksim_global.h"
//flshsim
#include "ssd_interface.h"
#include "flash.h"
#include "dftl.h"

extern int warm_flag;
void warmFlashsynth(){

  memset(dm_table, -1, sizeof(int) * DM_MGR_SIZE);

  nand_stat_reset();
  reset_flash_stat();

  if(ftl_type == 3){
    opagemap_reset();
  }

  else if(ftl_type == 4)
  {
    write_count = 0;
    read_count = 0;
  }
}


void MywarmFlash(){
//    因为预取策略所以最好的预热方式是按flash盘的大小全部写入一遍数据
    long int blkno,SLC_blkno,MLC_blkno;
    double delay;
    int cacheindex;
    int i;
    int req_page_num;
    int MLC_req_sec_limit=total_MLC_sect_num;
    /*4k对齐*/
    int MLC_req_page_limit=total_MLC_sect_num/8;
    for(req_page_num=0;req_page_num<=MLC_req_page_limit;pagemap_num++){
        SLC_blkno=req_page_num;
        MLC_blkno=req_page_num;

        MLC_blkno /= 8;
        MLC_blkno *= 8;

        delay=callFsim(MLC_blkno,8,0,1);
        dm_table[req_page_num]=DEV_FLASH;

    }
    nand_stat_reset();

    if(ftl_type == 3)
    {
        opagemap_reset();
        /*  init_arr();
             free(real_arr);
             //free(ghost_arr);
             free(seq_arr);
             free(second_arr);
          MAP_REAL_NUM_ENTRIES = 0;
          //MAP_GHOST_NUM_ENTRIES = 0;
          MAP_SEQ_NUM_ENTRIES = 0;
          MAP_SECOND_NUM_ENTRIES = 0;*/
        rqst_cnt = 0;
        read_count=0;
        write_count=0;
        trace_num = 0;
        translation_read_num = 0;
        translation_write_num = 0;
        cacheindex = MLC_TOTAL_MAP_ENTRIES;
        write_cnt=0;
        read_cnt=0;
        request_cnt=0;
        write_ratio=0;
        read_ratio=0;
        total_request_size=0;
        itemcount=0;
        average_request_size=0;
        /*     MAP_REAL_MAX_ENTRIES=0;
             //MAP_GHOST_MAX_ENTRIES=0;
             MAP_SEQ_MAX_ENTRIES=0;
             MAP_SECOND_MAX_ENTRIES=0; */
        in_cache_count = 0;
/*	while(cacheindex > 0)
	{
		cacheindex--;
		//MLC_opagemap[cacheindex].map_status = 0;
		MLC_opagemap[cacheindex].map_age = 0;
		MLC_opagemap[cacheindex].update = 0;
		MLC_opagemap[cacheindex].cache_status = 0;
		MLC_opagemap[cacheindex].cache_age = 0;
	}*/

    }
    else if(ftl_type == 4) {
        write_count = 0; read_count = 0; }


}


void warmFlash(char *tname){

  FILE *fp = fopen(tname, "r");
  char buffer[80];
  double time;
  int devno, bcount, flags,flash_flag,sbcount,mbcount;
  long int blkno,SLC_blkno,MLC_blkno;
  double delay;
  int i;
  int cacheindex;

  while(fgets(buffer, sizeof(buffer), fp)){
    sscanf(buffer, "%lf %d %d %d %d\n", &time, &devno, &blkno, &bcount, &flags);
    SLC_blkno=blkno;
    MLC_blkno=blkno;
  
//   无论读写都作为写请求写入，同时写放大系数放大到4倍
    mbcount = ((blkno + bcount -1) / 8 - (blkno)/8 + 1) * 8;
    MLC_blkno /= 8;
    MLC_blkno *= 8;

//   预热的时候增大请求的大小
    delay = callFsim(MLC_blkno, mbcount*4, 0,1);


    for(i = blkno; i<(blkno+bcount); i++){ dm_table[i] = DEV_FLASH; }
  }
  nand_stat_reset();
  
  if(ftl_type == 3) 
  {
		opagemap_reset();
  /*  init_arr();
       free(real_arr);
       //free(ghost_arr);
       free(seq_arr);
       free(second_arr);
	MAP_REAL_NUM_ENTRIES = 0;
	//MAP_GHOST_NUM_ENTRIES = 0;
	MAP_SEQ_NUM_ENTRIES = 0;
	MAP_SECOND_NUM_ENTRIES = 0;*/
	rqst_cnt = 0;
       read_count=0;
       write_count=0;
       trace_num = 0;
	translation_read_num = 0;
	translation_write_num = 0;
	cacheindex = MLC_TOTAL_MAP_ENTRIES;
       write_cnt=0;
       read_cnt=0;
       request_cnt=0;
       write_ratio=0;
       read_ratio=0;
       total_request_size=0;
       itemcount=0;
       average_request_size=0;
  /*     MAP_REAL_MAX_ENTRIES=0;
       //MAP_GHOST_MAX_ENTRIES=0;
       MAP_SEQ_MAX_ENTRIES=0; 
       MAP_SECOND_MAX_ENTRIES=0; */
       in_cache_count = 0;
/*	while(cacheindex > 0)
	{
		cacheindex--;
		//MLC_opagemap[cacheindex].map_status = 0;
		MLC_opagemap[cacheindex].map_age = 0;
		MLC_opagemap[cacheindex].update = 0;
		MLC_opagemap[cacheindex].cache_status = 0;
		MLC_opagemap[cacheindex].cache_age = 0;
	}*/
 
  }
  else if(ftl_type == 4) {
    write_count = 0; read_count = 0; }

    warm_flag=0;
  fclose(fp);
}

int main (int argc, char **argv)
{
  int i;
  int len;
  void *addr;
  void *newaddr;


  if(argc == 2) {
     disksim_restore_from_checkpoint (argv[1]);
  } 
  else {
    len = 8192000 + 2048000 + ALLOCSIZE;
    addr = malloc (len);
    newaddr = (void *) (rounduptomult ((long)addr, ALLOCSIZE));
    len -= ALLOCSIZE;

    disksim = disksim_initialize_disksim_structure (newaddr, len);
    disksim_setup_disksim (argc, argv);
  }

  memset(dm_table, -1, sizeof(int)*DM_MGR_SIZE);

  if(ftl_type != -1){

    initFlash();
    reset_flash_stat();
    reset_SLC_flash_stat();
    reset_MLC_flash_stat();
    nand_stat_reset();
  }

  warm_flag=1;
 // warmFlashsynth();
  warmFlash(argv[4]);
//    MywarmFlash();
  disksim_run_simulation ();

  disksim_cleanup_and_printstats ();

  return 0;
}
