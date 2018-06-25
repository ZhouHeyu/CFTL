/* 
 * Contributors: Youngjae Kim (youkim@cse.psu.edu)
 *               Aayush Gupta (axg354@cse.psu.edu)
 *
 * In case if you have any doubts or questions, kindly write to: youkim@cse.psu.edu 
 *
 * Description: This is a header file for flash.c.
 *
 * Acknowledgement: We thank Jeong Uk Kang by sharing the initial version 
 * of sector-level FTL source code. 
 *
 */

#ifndef SSD_LAYOUT 
#define SSD_LAYOUT 

#include  "type.h"

#define LB_SIZE_512  1
#define LB_SIZE_1024 2
#define LB_SIZE_2048 4

#define SECT_NUM_PER_PAGE  4
#define PAGE_NUM_PER_BLK  64
#define SECT_NUM_PER_BLK  (SECT_NUM_PER_PAGE * PAGE_NUM_PER_BLK)
#define SECT_SIZE_B 512

#define SECT_BITS       2
#define PAGE_BITS       6
#define PAGE_SECT_BITS  8
#define BLK_BITS       24

#define NAND_STATE_FREE    -1
#define NAND_STATE_INVALID -2

#define SECT_MASK_IN_SECT 0x0003
#define PAGE_MASK_IN_SECT 0x00FC
#define PAGE_SECT_MASK_IN_SECT 0x00FF
#define BLK_MASK_IN_SECT  0xFFFFFF00
#define PAGE_BITS_IN_PAGE 0x003F
#define BLK_MASK_IN_PAGE  0x3FFFFFC0

#define PAGE_SIZE_B (SECT_SIZE_B * SECT_NUM_PER_PAGE)
#define PAGE_SIZE_KB (PAGE_SIZE_B / 1024)
#define BLK_SIZE_B  (PAGE_SIZE_B * PAGE_NUM_PER_BLK)
#define BLK_SIZE_KB (BLK_SIZE_B / 1024)

#define BLK_NO_SECT(sect)  (((sect) & BLK_MASK_IN_SECT) >> (PAGE_BITS + SECT_BITS))
#define PAGE_NO_SECT(sect) (((sect) & PAGE_MASK_IN_SECT) >> SECT_BITS)
#define SECT_NO_SECT(sect) ((sect) & SECT_MASK_IN_SECT)
#define BLK_PAGE_NO_SECT(sect) ((sect) >> SECT_BITS)
#define PAGE_SECT_NO_SECT(sect) ((sect) & PAGE_SECT_MASK_IN_SECT)
#define BLK_NO_PAGE(page)  (((page) & BLK_MASK_IN_PAGE) >> PAGE_BITS)
#define PAGE_NO_PAGE(page) ((page) & PAGE_MASK_IN_PAGE)
#define SECTOR(blk, page) (((blk) << PAGE_SECT_BITS) | (page))

#define BLK_MASK_SECT 0x3FFFFF00
#define PGE_MASK_SECT 0x000000FC
#define OFF_MASK_SECT 0x00000003
#define IND_MASK_SECT (PGE_MASK_SECT | OFF_MASK_SECT)
#define BLK_BITS_SECT 22
#define PGE_BITS_SECT  6
#define OFF_BITS_SECT  2
#define IND_BITS_SECT (PGE_BITS_SECT + OFF_BITS_SECT)
#define BLK_F_SECT(sect) (((sect) & BLK_MASK_SECT) >> IND_BITS_SECT)
#define PGE_F_SECT(sect) (((sect) & PGE_MASK_SECT) >> OFF_BITS_SECT)
#define OFF_F_SECT(sect) (((sect) & OFF_MASK_SECT))
#define PNI_F_SECT(sect) (((sect) & (~OFF_MASK_SECT)) >> OFF_BITS_SECT)
#define IND_F_SECT(sect) (((sect) & IND_MASK_SECT))
#define IS_SAME_BLK(s1, s2) (((s1) & BLK_MASK_SECT) == ((s2) & BLK_MASK_SECT))
#define IS_SAME_PAGE(s1, s2) (((s1) & (~OFF_MASK_SECT)) == ((s2) & (~OFF_MASK_SECT)))


/***************SLC flah******************/
#define S_SECT_NUM_PER_PAGE  4
#define S_PAGE_NUM_PER_BLK  64
#define S_SECT_NUM_PER_BLK  (S_SECT_NUM_PER_PAGE * S_PAGE_NUM_PER_BLK)
#define SECT_SIZE_B 512

#define S_SECT_BITS       2
#define S_PAGE_BITS       6
#define S_PAGE_SECT_BITS  8
#define S_BLK_BITS       24

#define NAND_STATE_FREE    -1
#define NAND_STATE_INVALID -2

#define S_SECT_MASK_IN_SECT 0x0003
#define S_PAGE_MASK_IN_SECT 0x00FC
#define S_PAGE_SECT_MASK_IN_SECT 0x00FF
#define S_BLK_MASK_IN_SECT  0xFFFFFF00
#define S_PAGE_BITS_IN_PAGE 0x003F
#define S_BLK_MASK_IN_PAGE  0x3FFFFFC0

#define S_PAGE_SIZE_B (SECT_SIZE_B * S_SECT_NUM_PER_PAGE)
#define S_PAGE_SIZE_KB (S_PAGE_SIZE_B / 1024)
#define S_BLK_SIZE_B  (S_PAGE_SIZE_B * S_PAGE_NUM_PER_BLK)
#define S_BLK_SIZE_KB (S_BLK_SIZE_B / 1024)

#define S_BLK_NO_SECT(sect)  (((sect) & S_BLK_MASK_IN_SECT) >> (S_PAGE_BITS + S_SECT_BITS))
#define S_PAGE_NO_SECT(sect) (((sect) & S_PAGE_MASK_IN_SECT) >> S_SECT_BITS)
#define S_SECT_NO_SECT(sect) ((sect) & S_SECT_MASK_IN_SECT)
#define S_BLK_PAGE_NO_SECT(sect) ((sect) >> S_SECT_BITS)
#define S_PAGE_SECT_NO_SECT(sect) ((sect) & S_PAGE_SECT_MASK_IN_SECT)
#define S_BLK_NO_PAGE(page)  (((page) & S_BLK_MASK_IN_PAGE) >> S_PAGE_BITS)
#define S_PAGE_NO_PAGE(page) ((page) & S_PAGE_MASK_IN_PAGE)
#define S_SECTOR(blk, page) (((blk) << S_PAGE_SECT_BITS) | (page))

#define S_BLK_MASK_SECT 0x3FFFFF00
#define S_PGE_MASK_SECT 0x000000FC
#define S_OFF_MASK_SECT 0x00000003
#define S_IND_MASK_SECT (S_PGE_MASK_SECT | S_OFF_MASK_SECT)
#define S_BLK_BITS_SECT 22
#define S_PGE_BITS_SECT  6
#define S_OFF_BITS_SECT  2
#define S_IND_BITS_SECT (S_PGE_BITS_SECT + S_OFF_BITS_SECT)
#define S_BLK_F_SECT(sect) (((sect) & S_BLK_MASK_SECT) >> S_IND_BITS_SECT)
#define S_PGE_F_SECT(sect) (((sect) & S_PGE_MASK_SECT) >> S_OFF_BITS_SECT)
#define S_OFF_F_SECT(sect) (((sect) & S_OFF_MASK_SECT))
#define S_PNI_F_SECT(sect) (((sect) & (~S_OFF_MASK_SECT)) >> S_OFF_BITS_SECT)
#define S_IND_F_SECT(sect) (((sect) & S_IND_MASK_SECT))
#define S_IS_SAME_BLK(s1, s2) (((s1) & S_BLK_MASK_SECT) == ((s2) & S_BLK_MASK_SECT))
#define S_IS_SAME_PAGE(s1, s2) (((s1) & (~S_OFF_MASK_SECT)) == ((s2) & (~S_OFF_MASK_SECT)))
/*****************************************/

/***************MLC flah******************/
#define M_SECT_NUM_PER_PAGE  8
#define M_PAGE_NUM_PER_BLK  128
#define M_SECT_NUM_PER_BLK  (M_SECT_NUM_PER_PAGE * M_PAGE_NUM_PER_BLK)


#define M_SECT_BITS       3
#define M_PAGE_BITS       7
#define M_PAGE_SECT_BITS  10
#define M_BLK_BITS       22

#define M_SECT_MASK_IN_SECT 0x0007
#define M_PAGE_MASK_IN_SECT 0x03F8;
#define M_PAGE_SECT_MASK_IN_SECT 0x03FF //wfk:一个块中所有的扇区数
#define M_BLK_MASK_IN_SECT  0xFFFFFC00//0xFFEF7C00  0xFFEFFC00 wfk:从块号为4096开始
#define M_PAGE_BITS_IN_PAGE 0x007F //wfk:128个页／block
#define M_BLK_MASK_IN_PAGE  0x3FFFFF80

#define M_PAGE_SIZE_B (SECT_SIZE_B * M_SECT_NUM_PER_PAGE)
#define M_PAGE_SIZE_KB (M_PAGE_SIZE_B / 1024)
#define M_BLK_SIZE_B  (M_PAGE_SIZE_B * M_PAGE_NUM_PER_BLK)
#define M_BLK_SIZE_KB (M_BLK_SIZE_B / 1024)

#define M_BLK_NO_SECT(sect)  (((sect) & M_BLK_MASK_IN_SECT) >> (M_PAGE_BITS + M_SECT_BITS))
#define M_PAGE_NO_SECT(sect) (((sect) & M_PAGE_MASK_IN_SECT) >> M_SECT_BITS)
#define M_SECT_NO_SECT(sect) ((sect) & M_SECT_MASK_IN_SECT)
#define M_BLK_PAGE_NO_SECT(sect) ((sect) >> M_SECT_BITS)
#define M_PAGE_SECT_NO_SECT(sect) ((sect) & M_PAGE_SECT_MASK_IN_SECT)
#define M_BLK_NO_PAGE(page)  (((page) & M_BLK_MASK_IN_PAGE) >> M_PAGE_BITS)
#define M_PAGE_NO_PAGE(page) ((page) & M_PAGE_MASK_IN_PAGE)
#define M_SECTOR(blk, page) (((blk) << M_PAGE_SECT_BITS) | (page))

#define M_BLK_MASK_SECT 0x3FFFFC00//0x3FEF7C00
#define M_PGE_MASK_SECT 0x000003F8
#define M_OFF_MASK_SECT 0x00000007
#define M_IND_MASK_SECT (M_PGE_MASK_SECT | M_OFF_MASK_SECT)
#define M_BLK_BITS_SECT 22
#define M_PGE_BITS_SECT  7
#define M_OFF_BITS_SECT  3
#define M_IND_BITS_SECT (M_PGE_BITS_SECT + M_OFF_BITS_SECT)
#define M_BLK_F_SECT(sect) (((sect) & M_BLK_MASK_SECT) >> M_IND_BITS_SECT)
#define M_PGE_F_SECT(sect) (((sect) & M_PGE_MASK_SECT) >> M_OFF_BITS_SECT)
#define M_OFF_F_SECT(sect) (((sect) & M_OFF_MASK_SECT))
#define M_PNI_F_SECT(sect) (((sect) & (~M_OFF_MASK_SECT)) >> M_OFF_BITS_SECT)
#define M_IND_F_SECT(sect) (((sect) & M_IND_MASK_SECT))
#define M_IS_SAME_BLK(s1, s2) (((s1) & M_BLK_MASK_SECT) == ((s2) & M_BLK_MASK_SECT))
#define M_IS_SAME_PAGE(s1, s2) (((s1) & (~M_OFF_MASK_SECT)) == ((s2) & (~M_OFF_MASK_SECT)))
/*****************************************/
struct blk_state {
   int free;
   int ec;
   int update_ec;
};
struct sect_state {
  _u32 free  :  1;
  _u32 valid :  1;
  _u32 lsn   : 30;
};

struct nand_blk_info {
  struct blk_state state;                   // Erase Conunter
  struct sect_state sect[SECT_NUM_PER_BLK]; // Logical Sector Number
  _s32 fpc : 10; // free page counter
  _s32 ipc : 10; // invalide page counter
  _s32 lwn : 12; // last written page number
  int page_status[PAGE_NUM_PER_BLK];
};
struct SLC_nand_blk_info {
  struct blk_state state;                   // Erase Conunter
  struct sect_state sect[S_SECT_NUM_PER_BLK]; // Logical Sector Number
  _s32 fpc : 10; // free page counter
  _s32 ipc : 10; // invalide page counter
  _s32 lwn : 12; // last written page number
  int page_status[S_PAGE_NUM_PER_BLK];
};
struct MLC_nand_blk_info {
  struct blk_state state;                   // Erase Conunter
  struct sect_state sect[M_SECT_NUM_PER_BLK]; // Logical Sector Number
  _s32 fpc : 21; // free page counter
  _s32 ipc : 21; // invalide page counter
  _s32 lwn : 20; // last written page number
  int page_status[M_PAGE_NUM_PER_BLK];
};
extern _u32 nand_blk_num;
extern _u32 nand_SLC_blk_num;
extern _u32 nand_MLC_blk_num;
extern _u8  pb_size;
extern struct nand_blk_info *nand_blk;
extern struct SLC_nand_blk_info *SLC_nand_blk;
extern struct MLC_nand_blk_info *MLC_nand_blk;
extern FILE *fp_erase;
int nand_init (_u32 blk_num,_u32 SLC_blk_num,_u32 MLC_blk_num, _u8 min_free_blk_num);
void nand_end ();
_u8 nand_page_read (_u32 psn, _u32 *lsns, _u8 isGC);
_u8 MLC_nand_page_read(_u32 psn, _u32 *lsns, _u8 isGC);
_u8 SLC_nand_page_read(_u32 psn, _u32 *lsns, _u8 isGC);
_u8 nand_page_write (_u32 psn, _u32 *lsns, _u8 isGC, int map_flag);
_u8 SLC_nand_page_write(_u32 psn, _u32 *lsns, _u8 isGC, int map_flag);
_u8 MLC_nand_page_write(_u32 psn, _u32 *lsns, _u8 isGC, int map_flag);
void nand_erase (_u32 blk_no);
void MLC_nand_erase (_u32 blk_no);
void SLC_nand_erase (_u32 blk_no);
void nand_invalidate (_u32 psn, _u32 lsn);
void MLC_nand_invalidate (_u32 psn, _u32 lsn);
void SLC_nand_invalidate (_u32 psn, _u32 lsn);
_u32 nand_get_SLC_free_blk (int isGC); 
_u32 nand_get_MLC_free_blk (int isGC); 
_u32 nand_get_free_blk (int );
void nand_stat(int);
void nand_stat_reset();
void nand_stat_print(FILE *outFP);
int nand_oob_read(_u32 psn);
int MLC_nand_oob_read(_u32 psn);
int SLC_nand_oob_read(_u32 psn);

_u32 free_blk_num;
_u32 free_SLC_blk_num;
_u32 free_MLC_blk_num;
_u32 free_blk_idx;

_u32 stat_read_num, stat_write_num, stat_erase_num;
_u32 stat_gc_read_num, stat_gc_write_num;
_u32 stat_oob_read_num, stat_oob_write_num;

_u32 SLC_stat_read_num, SLC_stat_write_num, SLC_stat_erase_num;
_u32 SLC_stat_gc_read_num, SLC_stat_gc_write_num;
_u32 SLC_stat_oob_read_num, SLC_stat_oob_write_num;

_u32 MLC_stat_read_num, MLC_stat_write_num, MLC_stat_erase_num;
_u32 MLC_stat_gc_read_num, MLC_stat_gc_write_num;
_u32 MLC_stat_oob_read_num, MLC_stat_oob_write_num;
#endif 

#define WEAR_LEVEL_THRESHOLD   35 

void flush(int); 
