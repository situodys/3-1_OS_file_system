#ifndef __HW1_H__
#define __HW1_H__

#include "disk.h"

// ------- Caution -----------------------------------------
#define FS_DISK_CAPACITY (BLOCK_SIZE * 512)

#define NUM_OF_DIRECT_BLOCK_PTR (4) /* direct block pointer의 개수 */
#define MAX_NAME_LEN (60)

#define FILESYS_INFO_BLOCK (0)      /* file system info block no. */
#define INODE_BYTEMAP_BLOCK_NUM (1) /* inode bytemap block no. */
#define BLOCK_BYTEMAP_BLOCK_NUM (2) /* block bytemap block no. */
#define INODELIST_BLOCK_FIRST (3)   /* the first block no. of inode list */
#define INODELIST_BLOCKS (8)        /* the number of blocks in inode list */

#define INVALID_ENTRY (-1)
// ----------------------------------------------------------

typedef enum __fileType { FILE_TYPE_FILE, FILE_TYPE_DIR, FILE_TYPE_DEV } FileType;

typedef struct _Inode {
    int allocBlocks;
    int size;
    FileType type;
    int dirBlockPtr[NUM_OF_DIRECT_BLOCK_PTR]; // block pointers
    int indirectBlockPtr;                     // indirect block pointer
} Inode;

typedef struct __dirEntry {
    char name[MAX_NAME_LEN]; // file name
    int inodeNum;
} DirEntry;

/*
 * Functions implemented in HW1
 */
void FileSysInit(void);
void SetInodeBytemap(int inodeno);
void ResetInodeBytemap(int inodeno);
void SetBlockBytemap(int blkno);
void ResetBlockBytemap(int blkno);
void PutInode(int inodeno, Inode *pInode);
void GetInode(int inodeno, Inode *pInode);
int GetFreeInodeNum(void);
int GetFreeBlockNum(void);
void PutIndirectBlockEntry(int blkno, int index, int number);
int GetIndirectBlockEntry(int blkno, int index);
void RemoveIndirectBlockEntry(int blkno, int index);
void PutDirEntry(int blkno, int index, DirEntry *pEntry);
int GetDirEntry(int blkno, int index, DirEntry *pEntry);
void RemoveDirEntry(int blkno, int index);

#endif
