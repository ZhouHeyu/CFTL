/* 
 * Contributors: Youngjae Kim (youkim@cse.psu.edu)
 *               Aayush Gupta (axg354@cse.psu.edu)
 *
 * In case if you have any doubts or questions, kindly write to: youkim@cse.psu.edu 
 * 
 * Description: This is a header file for dftl.c.  
 * 
 */

#include "type.h"

#define MAP_INVALID 1 
#define MAP_REAL 2
#define MAP_GHOST 3
#define MAP_SEQ 3
//#define MAP_SECOND 5
#define MAP_SECOND 4

#define CACHE_INVALID 0
#define CACHE_VALID 1

int flash_hit;
int disk_hit;
int read_cache_hit;
int write_cache_hit;
int evict;
int update_reqd;
int delay_flash_update;
int save_count;
struct ftl_operation * opm_setup();

struct opm_entry {
  _u32 free  : 1;
  _u32 ppn   : 31;
  int  cache_status;
  int  cache_age;
  int  map_status;
  int  map_age;
  int  update;
};

struct omap_dir{
  unsigned int ppn;
};

#define MAP_ENTRIES_PER_PAGE 512
#define SLC_MAP_ENTRIES_PER_PAGE 512
#define MLC_MAP_ENTRIES_PER_PAGE 1024

int TOTAL_MAP_ENTRIES;
int SLC_TOTAL_MAP_ENTRIES;
int MLC_TOTAL_MAP_ENTRIES; 
int MAP_REAL_NUM_ENTRIES;
int MAP_SEQ_NUM_ENTRIES;
int MAP_SECOND_NUM_ENTRIES;

int CACHE_NUM_ENTRIES;

static int SYNC_NUM;

sect_t opagemap_num;
sect_t SLC_opagemap_num;
sect_t MLC_opagemap_num;
struct opm_entry *SLC_opagemap;
struct opm_entry *MLC_opagemap;
