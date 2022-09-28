#include "hw1.h"
#include "hw2.h"
#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#define DIR_NUM_MAX 100

FileSysInfo *pFileSysInfo;

void PrintFileSysInfo(void) {
    void *pBuf = NULL;

    pBuf = malloc(BLOCK_SIZE);
    DevReadBlock(0, pBuf);
    pFileSysInfo = (FileSysInfo *)pBuf;
    printf("File system info: # of allocated files:%d, # of allocated blocks:%d, #of free blocks:%d\n", pFileSysInfo->numAllocInodes, pFileSysInfo->numAllocBlocks, pFileSysInfo->numFreeBlocks);
    printf("\n");
}

void ListDirContents(char *dirName) {
    int i;
    int count;
    FileInfo *pFileInfo;
    Directory *pDir;

    // DirEntry pDirEntry[DIR_NUM_MAX];

    // EnumerateDirStatus(dirName, pDirEntry, &count);

    printf("[%s]Sub-directory:\n", dirName);
    pDir = OpenDirectory(dirName);
    while ((pFileInfo = ReadDirectory(pDir)) != NULL) {
        if (pFileInfo->filetype == FILE_TYPE_FILE) {
            printf("\t name:%s, start block:%d, type:file, blocks:%d, size:%d\n", pFileInfo->name, pFileInfo->inodeNum, pFileInfo->numBlocks, pFileInfo->size);
        } else if (pFileInfo->filetype == FILE_TYPE_DIR)
            printf("\t name:%s, start block:%d, type:directory, blocks:%d, size:%d\n", pFileInfo->name, pFileInfo->inodeNum, pFileInfo->numBlocks, pFileInfo->size);
        else {
            assert(0);
        }
    }
}

void TestCase1(void) {
    int i;
    char dirName[MAX_NAME_LEN] = {0};

    printf(" ---- Test Case 1 ----\n");

    MakeDirectory("/tmp");
    MakeDirectory("/usr");
    MakeDirectory("/etc");
    MakeDirectory("/home");
    /* make home directory */
    for (i = 0; i < 16; i++) {
        memset(dirName, 0, MAX_NAME_LEN);
        sprintf(dirName, "/home/u%d", i);
        MakeDirectory(dirName);
    }
    /* make etc directory */
    for (i = 0; i < 48; i++) {
        memset(dirName, 0, MAX_NAME_LEN);
        sprintf(dirName, "/etc/dev%d", i);
        MakeDirectory(dirName);
    }
    ListDirContents("/home");
    ListDirContents("/etc");

    /* remove subdirectory of etc directory */
    for (i = 47; i >= 0; i--) {
        memset(dirName, 0, MAX_NAME_LEN);
        sprintf(dirName, "/etc/dev%d", i);
        RemoveDirectory(dirName);
    }
    ListDirContents("/etc");
}

void TestCase2(void) {
    int i, j;
    int fd;
    char fileName[MAX_NAME_LEN];
    char dirName[MAX_NAME_LEN];

    printf(" ---- Test Case 2 ----\n");

    ListDirContents("/home");
    /* make home directory */
    for (i = 0; i < 4; i++) {
        for (j = 0; j < 16; j++) {
            memset(fileName, 0, MAX_NAME_LEN);
            sprintf(fileName, "/home/u%d/f%d", i, j);
            fd = OpenFile(fileName, OPEN_FLAG_CREATE);
            CloseFile(fd);
        }
    }

    for (i = 0; i < 4; i++) {
        memset(dirName, 0, MAX_NAME_LEN);
        sprintf(dirName, "/home/u%d", i);
        ListDirContents(dirName);
    }
}

void TestCase3(void) {
    int i, j;
    int fd;
    char fileName[MAX_NAME_LEN];
    char pBuffer1[BLOCK_SIZE];
    char pBuffer2[BLOCK_SIZE];

    printf(" ---- Test Case 3 ----\n");
    for (i = 0; i < 9; i++) {
        memset(fileName, 0, MAX_NAME_LEN);
        sprintf(fileName, "/home/u3/f%d", i);
        fd = OpenFile(fileName, OPEN_FLAG_CREATE);
        memset(pBuffer1, 0, BLOCK_SIZE);
        strcpy(pBuffer1, fileName);
        for (j = 0; j < 10; j++) {
            WriteFile(fd, pBuffer1, BLOCK_SIZE);
        }
        CloseFile(fd);
    }

    for (i = 0; i < 9; i++) {
        memset(fileName, 0, MAX_NAME_LEN);
        sprintf(fileName, "/home/u3/f%d", i);
        fd = OpenFile(fileName, OPEN_FLAG_CREATE);

        memset(pBuffer1, 0, BLOCK_SIZE);
        strcpy(pBuffer1, fileName);

        memset(pBuffer2, 0, BLOCK_SIZE);

        for (j = 0; j < 10; j++) {
            ReadFile(fd, pBuffer2, BLOCK_SIZE);

            if (strcmp(pBuffer1, pBuffer2)) {
                printf("TestCase 3: error\n");
                exit(0);
            }
        }
        CloseFile(fd);
    }
    printf(" ---- Test Case 3: files written/read----\n");
    ListDirContents("/home/u3");
}

void TestCase4(void) {
    int i, j;
    int fd;
    char fileName[MAX_NAME_LEN];
    char pBuffer1[BLOCK_SIZE * 2];
    char pBuffer2[BLOCK_SIZE * 2];

    printf(" ---- Test Case 4 ----\n");

    for (i = 0; i < 9; i++) {
        memset(fileName, 0, MAX_NAME_LEN);
        sprintf(fileName, "/home/u3/f%d", i);
        fd = OpenFile(fileName, OPEN_FLAG_TRUNCATE);

        memset(pBuffer1, 0, BLOCK_SIZE * 2);
        strcpy(pBuffer1, fileName);
        for (j = 0; j < 10; j++) {
            WriteFile(fd, pBuffer1, BLOCK_SIZE * 2);
        }
        CloseFile(fd);
    }

    printf(" ---- Test Case 4: truncated files are written ----\n");
    ListDirContents("/home/u3");

    for (i = 0; i < 9; i++) {
        memset(fileName, 0, MAX_NAME_LEN);
        sprintf(fileName, "/home/u3/f%d", i);
        fd = OpenFile(fileName, OPEN_FLAG_CREATE);

        memset(pBuffer1, 0, BLOCK_SIZE * 2);
        strcpy(pBuffer1, fileName);

        memset(pBuffer2, 0, BLOCK_SIZE * 2);
        for (j = 0; j < 10; j++) {
            ReadFile(fd, pBuffer2, BLOCK_SIZE * 2);

            if (strcmp(pBuffer1, pBuffer2)) {
                printf("TestCase 3: error\n");
                exit(0);
            }
        }
        CloseFile(fd);
    }

    printf(" ---- Test Case 4: truncated files are read ----\n");
    for (i = 0; i < 9; i++) {
        memset(fileName, 0, MAX_NAME_LEN);
        sprintf(fileName, "/home/u3/f%d", i);
        RemoveFile(fileName);
    }

    printf(" ---- Test Case 4: all files in /home/u3 are read ----\n");
    ListDirContents("/home/u3");

    printf(" ---- Test Case 4: /home/u3 directory is removed ----\n");
    RemoveDirectory("/home/u3");
    ListDirContents("/home");
}

int main(int argc, char **argv) {
    int TcNum;

    if (argc < 3) {
    ERROR:
        printf("usage: a.out [createfs| openfs] [1-5])\n");
        return -1;
    }

    if (strcmp(argv[1], "createfs") == 0)
        CreateFileSystem();
    else if (strcmp(argv[1], "openfs") == 0)
        OpenFileSystem();
    else
        goto ERROR;

    TcNum = atoi(argv[2]);

    DevResetDiskAccessCount();

    switch (TcNum) {
    case 1:
        TestCase1();
        PrintFileSysInfo();
        break;
    case 2:
        TestCase2();
        PrintFileSysInfo();
        break;
    case 3:
        TestCase3();
        PrintFileSysInfo();
        break;
    case 4:
        TestCase4();
        PrintFileSysInfo();
        break;
    default:
        CloseFileSystem();
        goto ERROR;
    }
    CloseFileSystem();

    printf("the number of disk access counts is %d\n", DevGetDiskReadCount() + DevGetDiskWriteCount());

    return 0;
}
