#ifndef __HW2_H__
#define __HW2_H__

#include "disk.h"
#include "hw1.h"


typedef int BOOL;

#define NUM_OF_DIRENT_PER_BLK   (BLOCK_SIZE / sizeof(DirEntry))
#define DESC_ENTRY_NUM      (128)
#define MAX_FILE_NUM        (128)
// ----------------------------------------------------------



/*
 * PPT에 있는 flag를 아래처럼 수정합니다.
 * CREATE는강의동영상에서 설명했습니다.
 * 나머지는 추후 공지에 올리겠ㅅ브니다.
 */
typedef enum __OpenFlag {
    OPEN_FLAG_TRUNCATE,
    OPEN_FLAG_CREATE,
    OPEN_FLAG_APPEND
} OpenFlag;


typedef struct _FileSysInfo {
    int blocks;              // 디스크에 저장된 전체 블록 개수
    int rootInodeNum;        // 루트 inode의 번호
    int diskCapacity;        // 디스크 용량 (Byte 단위)
    int numAllocBlocks;      // 파일 또는 디렉토리에 할당된 블록 개수
    int numFreeBlocks;       // 할당되지 않은 블록 개수
    int numAllocInodes;       // 할당된 inode 개수
    int blockBytemapBlock;     // block bytemap의 시작 블록 번호
    int inodeBytemapBlock;     // inode bytemap의 시작 블록 번호
    int inodeListBlock;     // inode list의 시작 블록 번호
    int dataRegionBlock;        // data region의 시작 블록 번호 (수정)
} FileSysInfo;


typedef struct __File {
    BOOL    bUsed;
    int     fileOffset;
    int     inodeNum;
} File;

typedef struct __FileTable {
    int         numUsedFile;
    File        pFile[MAX_FILE_NUM];
} FileTable;

typedef struct _DescEntry {
    int bUsed;
    /*
     * PPT에는 File object를 가리키도록 되어 있습니다.
     * 해당 File object를 가리키는 FileTable의 index를 가리키도록 합시다.
     */
    int fileTableIndex;   // index to file arrary in file table
} DescEntry;


typedef struct  __FileDescTable {
    int        numUsedDescEntry;
    DescEntry  pEntry[DESC_ENTRY_NUM];
} FileDescTable;


typedef struct __Directory {
    int         inodeNum;
} Directory;



typedef struct _FileInfo {
    char        name[MAX_NAME_LEN];
    FileType    filetype;
    int         inodeNum;
    int         numBlocks;
    int         size;
} FileInfo;


/*
 * Functions to be implemented in HW2
 */
int         OpenFile(const char* name, OpenFlag flag);
int         WriteFile(int fileDesc, char* pBuffer, int length);
int         ReadFile(int fileDesc, char* pBuffer, int length);
int         CloseFile(int fileDesc);
int         RemoveFile(char* name);
int         MakeDirectory(char* name);
int         RemoveDirectory(char* name);
void        CreateFileSystem(void);
void        OpenFileSystem(void);
void        CloseFileSystem(void);

Directory*  OpenDirectory(char* name);
FileInfo*   ReadDirectory(Directory* pDir);
int         CloseDirectory(Directory* pDir);


extern      FileDescTable* pFileDescTable;
extern      FileSysInfo* pFileSysInfo;

#endif


