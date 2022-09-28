#include "hw1.h"
#include "disk.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

const int FILE_SYS_LAST_METADATA_INDEX = 10;
const int MAX_INODE_NUMBER_PER_BLOCK = 16;
const int MAX_INODE_NUMBER = 128;
const int MAX_BLOCK_NUMBER = 512;

void FileSysInit(void) {
    //버퍼 초기화
    char *pBuf = malloc(BLOCK_SIZE * sizeof(char));
    memset(pBuf, 0, BLOCK_SIZE * sizeof(char));

    //가상 디스크를 생성.

    DevCreateDisk();

    //메타 데이터 초기화
    for (int i = 0; i <= FILE_SYS_LAST_METADATA_INDEX; i++) {
        DevWriteBlock(i, pBuf);
    }

    for (int i = 0; i <= FILE_SYS_LAST_METADATA_INDEX; i++) {
        SetBlockBytemap(i);
    }

    free(pBuf);
    return;
}

void SetInodeBytemap(int inodeno) {
    //버퍼 초기화
    char *pBuf = malloc(BLOCK_SIZE * sizeof(char));

    // InodeBytemap pBuf에 읽어옴
    DevReadBlock(INODE_BYTEMAP_BLOCK_NUM, pBuf);

    // inodeno 1로 설정
    *(pBuf + inodeno) = 1;

    // InodeBytemap에 변경값 저장
    DevWriteBlock(INODE_BYTEMAP_BLOCK_NUM, pBuf);

    free(pBuf);
}

void ResetInodeBytemap(int inodeno) {
    //버퍼 초기화
    char *pBuf = malloc(BLOCK_SIZE * sizeof(char));

    // InodeBytemap pBuf에 읽어옴
    DevReadBlock(INODE_BYTEMAP_BLOCK_NUM, pBuf);

    // inodeno 0로 설정
    *(pBuf + inodeno) = 0;

    // InodeBytemap에 변경값 저장
    DevWriteBlock(INODE_BYTEMAP_BLOCK_NUM, pBuf);

    free(pBuf);
}

void SetBlockBytemap(int blkno) {
    //버퍼 초기화
    char *pBuf = malloc(BLOCK_SIZE * sizeof(char));

    // BlockBytemap pBuf에 읽어옴
    DevReadBlock(BLOCK_BYTEMAP_BLOCK_NUM, pBuf);

    // BlockBytemap[blkno]  1로 설정
    *(pBuf + blkno) = 1;

    // BlockBytemap에 변경값 저장
    DevWriteBlock(BLOCK_BYTEMAP_BLOCK_NUM, pBuf);

    free(pBuf);
}

void ResetBlockBytemap(int blkno) {
    //버퍼 초기화
    char *pBuf = malloc(BLOCK_SIZE * sizeof(char));

    // BlockBytemap pBuf에 읽어옴
    DevReadBlock(BLOCK_BYTEMAP_BLOCK_NUM, pBuf);

    // BlockBytemap[blkno]  0로 설정
    *(pBuf + blkno) = 0;

    // BlockBytemap에 변경값 저장
    DevWriteBlock(BLOCK_BYTEMAP_BLOCK_NUM, pBuf);

    free(pBuf);
}

void PutInode(int inodeno, Inode *pInode) {
    // Inode 타겟 설정
    int blockNum = inodeno / MAX_INODE_NUMBER_PER_BLOCK + INODELIST_BLOCK_FIRST;
    int idxInBlock = inodeno % MAX_INODE_NUMBER_PER_BLOCK;

    //버퍼 초기화
    char *pBuf = malloc(BLOCK_SIZE * sizeof(char));

    // BlockBytemap pBuf에 읽어옴
    DevReadBlock(blockNum, pBuf);
    ((Inode *)pBuf + idxInBlock)->allocBlocks = pInode->allocBlocks;
    ((Inode *)pBuf + idxInBlock)->size = pInode->size;
    ((Inode *)pBuf + idxInBlock)->type = pInode->type;
    for (int i = 0; i < NUM_OF_DIRECT_BLOCK_PTR; i++) {
        ((Inode *)pBuf + idxInBlock)->dirBlockPtr[i] = pInode->dirBlockPtr[i];
    }
    ((Inode *)pBuf + idxInBlock)->indirectBlockPtr = pInode->indirectBlockPtr;

    DevWriteBlock(blockNum, pBuf);

    free(pBuf);
}

void GetInode(int inodeno, Inode *pInode) {
    // Inode 타겟 설정
    int blockNum = inodeno / MAX_INODE_NUMBER_PER_BLOCK + INODELIST_BLOCK_FIRST;
    int idxInBlock = inodeno % MAX_INODE_NUMBER_PER_BLOCK;

    //버퍼 초기화
    char *pBuf = malloc(BLOCK_SIZE * sizeof(char));

    // BlockBytemap pBuf에 읽어옴
    DevReadBlock(blockNum, pBuf);
    pInode->allocBlocks = ((Inode *)pBuf + idxInBlock)->allocBlocks;
    pInode->size = ((Inode *)pBuf + idxInBlock)->size;
    pInode->type = ((Inode *)pBuf + idxInBlock)->type;
    for (int i = 0; i < NUM_OF_DIRECT_BLOCK_PTR; i++) {
        pInode->dirBlockPtr[i] = ((Inode *)pBuf + idxInBlock)->dirBlockPtr[i];
    }
    pInode->indirectBlockPtr = ((Inode *)pBuf + idxInBlock)->indirectBlockPtr;

    free(pBuf);
}

int GetFreeInodeNum(void) {

    int EmptyInodeNum = -1;
    //버퍼 초기화
    char *pBuf = malloc(BLOCK_SIZE * sizeof(char));

    // InodeBytemap pBuf에 읽어옴
    DevReadBlock(INODE_BYTEMAP_BLOCK_NUM, pBuf);

    // 0번째부터 빈 inode번호를 찾는다
    for (int i = 0; i < MAX_INODE_NUMBER; i++) {
        if (*(pBuf + i) == 1)
            continue;
        EmptyInodeNum = i;
        break;
    }

    free(pBuf);
    return EmptyInodeNum;
}

int GetFreeBlockNum(void) {
    int EmptyBlockNum = -1;
    //버퍼 초기화
    char *pBuf = malloc(BLOCK_SIZE * sizeof(char));

    // BlockBytemap pBuf에 읽어옴
    DevReadBlock(BLOCK_BYTEMAP_BLOCK_NUM, pBuf);

    // 0번째부터 빈 inode번호를 찾는다
    for (int i = 0; i < MAX_BLOCK_NUMBER; i++) {
        if (*(pBuf + i) == 1)
            continue;
        EmptyBlockNum = i;
        break;
    }

    free(pBuf);
    return EmptyBlockNum;
}

void PutIndirectBlockEntry(int blkno, int index, int number) {
    //버퍼 초기화
    char *pBuf = malloc(BLOCK_SIZE * sizeof(char));

    DevReadBlock(blkno, pBuf);
    *((int *)pBuf + index) = number;
    DevWriteBlock(blkno, pBuf);

    free(pBuf);
}

int GetIndirectBlockEntry(int blkno, int index) {
    int IndirctBlockEntryNumber = -1;

    //버퍼 초기화
    char *pBuf = malloc(BLOCK_SIZE * sizeof(char));

    DevReadBlock(blkno, pBuf);
    IndirctBlockEntryNumber = *((int *)pBuf + index);

    free(pBuf);
    return IndirctBlockEntryNumber;
}

void RemoveIndirectBlockEntry(int blkno, int index) { PutIndirectBlockEntry(blkno, index, INVALID_ENTRY); }

void PutDirEntry(int blkno, int index, DirEntry *pEntry) {
    //버퍼 초기화
    char *pBuf = malloc(BLOCK_SIZE * sizeof(char));

    DevReadBlock(blkno, pBuf);
    ((DirEntry *)pBuf + index)->inodeNum = pEntry->inodeNum;
    memcpy(((DirEntry *)pBuf + index)->name, pEntry->name, strlen(pEntry->name) + 1);
    DevWriteBlock(blkno, pBuf);

    free(pBuf);
}

int GetDirEntry(int blkno, int index, DirEntry *pEntry) {
    //유효한지를 나타내는 flag 선언
    int isValid = 1;

    //버퍼 초기화
    char *pBuf = malloc(BLOCK_SIZE * sizeof(char));

    DevReadBlock(blkno, pBuf);
    pEntry->inodeNum = ((DirEntry *)pBuf + index)->inodeNum;
    memcpy(pEntry->name, ((DirEntry *)pBuf + index)->name, strlen(((DirEntry *)pBuf + index)->name) + 1);

    if (pEntry->inodeNum == INVALID_ENTRY)
        isValid = -1;

    free(pBuf);

    return isValid;
}

void RemoveDirEntry(int blkno, int index) {
    //버퍼 초기화
    char *pBuf = malloc(BLOCK_SIZE * sizeof(char));

    DevReadBlock(blkno, pBuf);
    ((DirEntry *)pBuf + index)->inodeNum = INVALID_ENTRY;
    DevWriteBlock(blkno, pBuf);

    free(pBuf);
}
