#include "hw2.h"
#include "disk.h"
#include "hw1.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

FileDescTable *pFileDescTable;
FileSysInfo *pFileSysInfo;
FileTable *pFileTable;
int cntDirIdx = 0;

void InitDirEntry(char *block) {

    for (int i = 0; i < NUM_OF_DIRENT_PER_BLK; i++) {
        ((DirEntry *)block + i)->inodeNum = -1;
    }
}

void InitInDirEntry(char *block) {
    for (int i = 0; i < 128; i++) {
        *((int *)block + i) = -1;
    }
}

int split(char *t, char **dest) {
    char name[strlen(t) + 1];
    strcpy(name, t);
    int count = 0;

    char *ptr = strtok(name, "/");

    for (int i = 0; ptr != NULL; i++) {
        dest[i] = malloc(sizeof(ptr) + 1);
        strcpy(dest[i], ptr);
        ptr = strtok(NULL, "/");
        count++;
    }
    return count;
}

int OpenFile(const char *name, OpenFlag flag) {
    int newFileInodeNum = GetFreeInodeNum();
    int rootInodeNum = 0;

    //경로관련
    char *pathList[512];
    int pathItemCnt = split(name, pathList);
    int cur = 0;
    // flag
    int isSameNameExist = 0;
    int newFileBlockLoc = -1;
    int newFileEntLoc = -1;
    int isFirstFreeEntry = 1;
    int isFileExist = 0;
    int parentInodeNum = -1;
    int existFileInode = -1;
    int freeFdEnt = -1;
    int fileTableIdx = -1;

    Inode *pInode = (Inode *)malloc(sizeof(Inode));
    memset(pInode, '\0', sizeof(Inode));
    Inode *checkInode = (Inode *)malloc(sizeof(Inode));
    memset(checkInode, '\0', sizeof(Inode));
    char *pBuf = malloc(BLOCK_SIZE * sizeof(char));
    memset(pBuf, '\0', BLOCK_SIZE * sizeof(char));
    char *fileSysInfo = malloc(BLOCK_SIZE * sizeof(char));
    memset(fileSysInfo, '\0', BLOCK_SIZE * sizeof(char));
    DevReadBlock(0, fileSysInfo);

    GetInode(rootInodeNum, pInode);

    DirEntry *pDirEntry = (DirEntry *)malloc(sizeof(DirEntry));
    memset(pDirEntry, '\0', sizeof(DirEntry));

    while (cur < pathItemCnt) {
        newFileBlockLoc = -1;
        newFileEntLoc = -1;
        isFirstFreeEntry = 1;

        for (int dirBlockPtrIdx = 0; dirBlockPtrIdx < pInode->allocBlocks; dirBlockPtrIdx++) {
            for (int dirEntIdx = 0; dirEntIdx < NUM_OF_DIRENT_PER_BLK; dirEntIdx++) {
                memset(pDirEntry, '\0', sizeof(DirEntry));
                if (GetDirEntry(pInode->dirBlockPtr[dirBlockPtrIdx], dirEntIdx, pDirEntry) == -1) {
                    if (isFirstFreeEntry) {
                        newFileBlockLoc = pInode->dirBlockPtr[dirBlockPtrIdx];
                        newFileEntLoc = dirEntIdx;
                        isFirstFreeEntry = 0;
                    }
                    continue;
                }

                if (strcmp(pDirEntry->name, pathList[cur]) == 0) {
                    GetInode(pDirEntry->inodeNum, checkInode);
                    if (checkInode->type == FILE_TYPE_FILE) {
                        if (cur == pathItemCnt - 1) {
                            isFileExist = 1;
                            existFileInode = pDirEntry->inodeNum;
                            isSameNameExist = 1;
                            break;
                        }
                    } else if (checkInode->type == FILE_TYPE_DIR) {
                        if (cur != pathItemCnt - 1) {
                            isSameNameExist = 1;
                            break;
                        }
                    }
                }

                if (strcmp(pDirEntry->name, ".") == 0 && (cur == pathItemCnt - 1)) {
                    parentInodeNum = pDirEntry->inodeNum;
                }
            }
            if (isSameNameExist) {
                if (isFileExist)
                    break;
                GetInode(pDirEntry->inodeNum, pInode);
                break;
            }
        }
        if (isSameNameExist) {
            isSameNameExist = 0;
            cur++;
            continue;
        } else {
            if (pInode->indirectBlockPtr < 1) {
                if (cur != pathItemCnt - 1) {
                    free(pBuf);
                    free(pInode);
                    free(pDirEntry);
                    free(checkInode);
                    return -1;
                }
                cur++;
            } else {
                int dirBlockNum = -1;
                for (int dirBlockIdx = 0; dirBlockIdx < BLOCK_SIZE / sizeof(int); dirBlockIdx++) {
                    dirBlockNum = GetIndirectBlockEntry(pInode->indirectBlockPtr, dirBlockIdx);
                    if (dirBlockNum < 1)
                        continue;

                    for (int dirEntIdx = 0; dirEntIdx < NUM_OF_DIRENT_PER_BLK; dirEntIdx++) {
                        if (GetDirEntry(dirBlockNum, dirEntIdx, pDirEntry) == -1) {
                            if (isFirstFreeEntry) {
                                newFileBlockLoc = dirBlockNum;
                                newFileEntLoc = dirEntIdx;
                                isFirstFreeEntry = 0;
                            }
                            continue;
                        }
                        if (strcmp(pDirEntry->name, pathList[cur]) == 0) {
                            GetInode(pDirEntry->inodeNum, checkInode);
                            if (checkInode->type == FILE_TYPE_FILE) {
                                if (cur == pathItemCnt - 1) {
                                    isFileExist = 1;
                                    existFileInode = pDirEntry->inodeNum;
                                    isSameNameExist = 1;
                                    break;
                                }
                            } else if (checkInode->type == FILE_TYPE_DIR) {
                                if (cur != pathItemCnt - 1) {
                                    isSameNameExist = 1;
                                    break;
                                }
                            }
                        }
                    }
                    if (isSameNameExist) {
                        if (isFileExist)
                            break;
                        GetInode(pDirEntry->inodeNum, pInode);
                        break;
                    }
                }
                if (isSameNameExist) {
                    isSameNameExist = 0;
                    cur++;
                    continue;
                } else if (cur != pathItemCnt - 1)
                    return -1;
                else
                    cur++;
            }
        }
    }

    if (!isFileExist) {
        if (flag == OPEN_FLAG_CREATE) {
            if (newFileBlockLoc == -1 || newFileEntLoc == -1) {
                newFileBlockLoc = GetFreeBlockNum();
                newFileEntLoc = 0;

                // dirBlock 생성 후 저장
                char *pblk = malloc(BLOCK_SIZE * sizeof(char));
                memset(pblk, '\0', BLOCK_SIZE * sizeof(char));
                InitDirEntry(pblk);
                DevWriteBlock(newFileBlockLoc, pblk);
                ((FileSysInfo *)fileSysInfo)->numAllocBlocks++;
                ((FileSysInfo *)fileSysInfo)->numFreeBlocks--;
                pInode->allocBlocks++;
                pInode->size += 512;
                PutInode(parentInodeNum, pInode);
                free(pblk);
                SetBlockBytemap(newFileBlockLoc);

                // Inode에 생성한 블럭과 정보 저장

                // indirectBlockPtr에 저장
                if (pInode->indirectBlockPtr > 0) {
                    for (int i = 0; i < 128; i++) {
                        if (GetIndirectBlockEntry(pInode->indirectBlockPtr, i) == -1) {
                            PutIndirectBlockEntry(pInode->indirectBlockPtr, i, newFileBlockLoc);
                            break;
                        }
                    }
                } else {
                    if (pInode->allocBlocks == 5) {
                        char *pInDblk = malloc(BLOCK_SIZE * sizeof(char));
                        memset(pInDblk, '\0', BLOCK_SIZE * sizeof(char));
                        InitInDirEntry(pInDblk);
                        int inDirBlkNo = GetFreeBlockNum();
                        DevWriteBlock(inDirBlkNo, pInDblk);
                        SetBlockBytemap(inDirBlkNo);
                        ((FileSysInfo *)fileSysInfo)->numAllocBlocks++;
                        ((FileSysInfo *)fileSysInfo)->numFreeBlocks--;
                        pInode->indirectBlockPtr = inDirBlkNo;
                        PutInode(parentInodeNum, pInode);
                        PutIndirectBlockEntry(pInode->indirectBlockPtr, 0, newFileBlockLoc);
                    } else {
                        pInode->dirBlockPtr[pInode->allocBlocks - 1] = newFileBlockLoc;
                        PutInode(parentInodeNum, pInode);
                    }
                }
            }

            pDirEntry->inodeNum = newFileInodeNum;
            memcpy(pDirEntry->name, pathList[pathItemCnt - 1], strlen(pathList[pathItemCnt - 1]));
            PutDirEntry(newFileBlockLoc, newFileEntLoc, pDirEntry);

            GetInode(newFileInodeNum, pInode);
            pInode->allocBlocks = 0;
            pInode->size = 0;
            pInode->type = FILE_TYPE_FILE;
            pInode->indirectBlockPtr = -1;
            PutInode(newFileInodeNum, pInode);

            SetInodeBytemap(newFileInodeNum);
            ((FileSysInfo *)fileSysInfo)->numAllocInodes++;
            DevWriteBlock(0, fileSysInfo);

            /*printf("blocks: %d\n rootInodeNum: %d\n diskCapacity: %d\n numAllocBlocks: %d\n numAllocInodes: %d\n numFreeBlocks: %d\n\n", ((FileSysInfo *)fileSysInfo)->blocks, ((FileSysInfo *)fileSysInfo)->rootInodeNum, ((FileSysInfo *)fileSysInfo)->diskCapacity,
                   ((FileSysInfo *)fileSysInfo)->numAllocBlocks, ((FileSysInfo *)fileSysInfo)->numAllocInodes, ((FileSysInfo *)fileSysInfo)->numFreeBlocks);*/

            File *newFile = malloc(sizeof(File));
            newFile->bUsed = 1;
            newFile->fileOffset = 0;
            newFile->inodeNum = newFileInodeNum;

            for (int i = 0; i < MAX_FILE_NUM; i++) {
                if (pFileTable->pFile[i].bUsed == 0) {
                    pFileTable->pFile[i] = *newFile;
                    pFileTable->numUsedFile++;
                    fileTableIdx = i;
                    break;
                }
            }

            for (int i = 0; i < DESC_ENTRY_NUM; i++) {
                if (pFileDescTable->pEntry[i].bUsed == 0) {
                    freeFdEnt = i;
                    pFileDescTable->pEntry[i].bUsed = 1;
                    pFileDescTable->pEntry[i].fileTableIndex = fileTableIdx;
                    pFileDescTable->numUsedDescEntry++;
                    break;
                }
            }
        } else {
            free(pInode);
            free(pBuf);
            free(fileSysInfo);
            free(checkInode);
            return -1;
        }
    } else {
        //파일이 이미존재한다면
        //열려 filetable에 존재하는지 확인
        //존재하면

        File *newFile = malloc(sizeof(File));
        newFile->bUsed = 1;
        newFile->fileOffset = 0;
        newFile->inodeNum = existFileInode;

        if (flag == OPEN_FLAG_TRUNCATE) {
            GetInode(existFileInode, pInode);
            int dblockno;
            for (int i = 0; i < NUM_OF_DIRECT_BLOCK_PTR; i++) {
                dblockno = pInode->dirBlockPtr[i];
                if (dblockno > 0) {
                    ResetBlockBytemap(dblockno);
                    pInode->dirBlockPtr[i] = -1;
                    ((FileSysInfo *)fileSysInfo)->numAllocBlocks--;
                    ((FileSysInfo *)fileSysInfo)->numFreeBlocks++;
                }
            }
            if (pInode->indirectBlockPtr > 0) {
                int idblockno;
                idblockno = pInode->indirectBlockPtr;
                for (int i = 0; i < 128; i++) {
                    dblockno = GetIndirectBlockEntry(idblockno, i);
                    if (dblockno > 0) {
                        ResetBlockBytemap(dblockno);
                        PutIndirectBlockEntry(idblockno, i, -1);
                        ((FileSysInfo *)fileSysInfo)->numAllocBlocks--;
                        ((FileSysInfo *)fileSysInfo)->numFreeBlocks++;
                    }
                }

                ResetBlockBytemap(idblockno);
                ((FileSysInfo *)fileSysInfo)->numAllocBlocks--;
                ((FileSysInfo *)fileSysInfo)->numFreeBlocks++;
            }
            pInode->allocBlocks = 0;
            pInode->indirectBlockPtr = -1;
            pInode->size = 0;
            PutInode(existFileInode, pInode);

            DevWriteBlock(0, fileSysInfo);
        }

        for (int i = 0; i < MAX_FILE_NUM; i++) {
            if (pFileTable->pFile[i].bUsed == 0) {
                pFileTable->pFile[i] = *newFile;
                pFileTable->numUsedFile++;
                fileTableIdx = i;
                free(newFile);
                break;
            }
        }

        for (int i = 0; i < DESC_ENTRY_NUM; i++) {
            if (pFileDescTable->pEntry[i].bUsed == 0) {
                freeFdEnt = i;
                pFileDescTable->pEntry[i].bUsed = 1;
                pFileDescTable->pEntry[i].fileTableIndex = fileTableIdx;
                pFileDescTable->numUsedDescEntry++;
                break;
            }
        }
    }

    free(pInode);
    free(pBuf);
    free(fileSysInfo);
    free(checkInode);
    return freeFdEnt;
}

int WriteFile(int fileDesc, char *pBuffer, int length) {
    if (fileDesc == -1)
        return -1;

    int loopCnt = 0;
    int tmp = length / BLOCK_SIZE;
    char *writeItem[tmp];
    while (tmp--) {
        writeItem[loopCnt] = malloc(BLOCK_SIZE * sizeof(char));
        memcpy(writeItem[loopCnt], pBuffer + BLOCK_SIZE * loopCnt, BLOCK_SIZE * sizeof(char));
        // printf("%d 번째 나눈 buffer값 %s\n", loopCnt, writeItem[loopCnt]);
        loopCnt++;
    }

    int writeItemIdx = 0;
    while (loopCnt--) {
        int newWriteBlockNum = GetFreeBlockNum();
        int fileInode = pFileTable->pFile[pFileDescTable->pEntry[fileDesc].fileTableIndex].inodeNum;
        int fileOffset = pFileTable->pFile[pFileDescTable->pEntry[fileDesc].fileTableIndex].fileOffset;
        int logicalDirBlockIdx = fileOffset / BLOCK_SIZE;

        Inode *pInode = (Inode *)malloc(sizeof(Inode));
        char *pBuf = malloc(BLOCK_SIZE * sizeof(char));
        char *fileSysInfo = malloc(BLOCK_SIZE * sizeof(char));
        memset(pInode, '\0', sizeof(Inode));
        memset(pBuf, '\0', BLOCK_SIZE * sizeof(char));
        memset(fileSysInfo, '\0', BLOCK_SIZE * sizeof(char));
        DevReadBlock(0, fileSysInfo);

        GetInode(fileInode, pInode);

        DirEntry *pDirEntry = (DirEntry *)malloc(sizeof(DirEntry));
        memset(pDirEntry, '\0', sizeof(DirEntry));
        /*
            if (logicalDirBlockIdx > 3) {
                pFileTable->pFile[pFileDescTable->pEntry[fileDesc].fileTableIndex].fileOffset += 512;
                if (pInode->indirectBlockPtr > 0) {
                    logicalDirBlockIdx -= 4;
                    int blkno = GetIndirectBlockEntry(pInode->indirectBlockPtr, logicalDirBlockIdx);
                    if (blkno == -1) {
                        PutIndirectBlockEntry(pInode->indirectBlockPtr, logicalDirBlockIdx, newWriteBlockNum);
                        SetBlockBytemap(newWriteBlockNum);
                        ((FileSysInfo *)fileSysInfo)->numAllocBlocks++;
                        ((FileSysInfo *)fileSysInfo)->numFreeBlocks--;
                        DevWriteBlock(newWriteBlockNum, pBuffer);
                    } else {
                        DevWriteBlock(blkno, pBuffer);
                    }
                    return length < 512 ? length : 512;
                } else {
                    int indirectBlockNum = newWriteBlockNum;
                    char *pInDblk = malloc(BLOCK_SIZE * sizeof(char));
                    memset(pInDblk, '\0', BLOCK_SIZE * sizeof(char));
                    InitInDirEntry(pInDblk);
                    DevWriteBlock(indirectBlockNum, pInDblk);
                    SetBlockBytemap(indirectBlockNum);
                    ((FileSysInfo *)fileSysInfo)->numAllocBlocks++;
                    ((FileSysInfo *)fileSysInfo)->numFreeBlocks--;
                    pInode->indirectBlockPtr = indirectBlockNum;
                    PutInode(fileInode, pInode);

                    newWriteBlockNum = GetFreeBlockNum();
                    SetBlockBytemap(newWriteBlockNum);
                    ((FileSysInfo *)fileSysInfo)->numAllocBlocks++;
                    ((FileSysInfo *)fileSysInfo)->numFreeBlocks--;
                    PutIndirectBlockEntry(pInode->indirectBlockPtr, 0, newWriteBlockNum);

                    DevWriteBlock(newWriteBlockNum, pBuffer);
                    free(pInDblk);
                    return length < 512 ? length : 512;
                }
            } else {
                pFileTable->pFile[pFileDescTable->pEntry[fileDesc].fileTableIndex].fileOffset += 512;
                if (pInode->dirBlockPtr[logicalDirBlockIdx] > 1) {
                    DevWriteBlock(pInode->dirBlockPtr[logicalDirBlockIdx], pBuffer);
                } else {
                    pInode->allocBlocks++;
                    pInode->size += 512;
                    pInode->dirBlockPtr[logicalDirBlockIdx] = newWriteBlockNum;
                    PutInode(fileInode, pInode);
                    SetBlockBytemap(newWriteBlockNum);
                    ((FileSysInfo *)fileSysInfo)->numAllocBlocks++;
                    ((FileSysInfo *)fileSysInfo)->numFreeBlocks--;
                    DevWriteBlock(0, fileSysInfo);
                    DevWriteBlock(newWriteBlockNum, pBuffer);
                }

                return length < 512 ? length : 512;
            }
        */
        if (pInode->indirectBlockPtr > 0) {
            int indirBlkno = pInode->indirectBlockPtr;
            logicalDirBlockIdx -= 4;
            if (GetIndirectBlockEntry(indirBlkno, logicalDirBlockIdx) == -1) {
                DevWriteBlock(newWriteBlockNum, writeItem[writeItemIdx]);
                SetBlockBytemap(newWriteBlockNum);
                PutIndirectBlockEntry(indirBlkno, logicalDirBlockIdx, newWriteBlockNum);
                ((FileSysInfo *)fileSysInfo)->numAllocBlocks++;
                ((FileSysInfo *)fileSysInfo)->numFreeBlocks--;
                pInode->allocBlocks++;
                pInode->size += 512;
                PutInode(fileInode, pInode);
            } else
                DevWriteBlock(GetIndirectBlockEntry(indirBlkno, logicalDirBlockIdx), writeItem[writeItemIdx]);
        } else {
            if (pInode->allocBlocks == 4) {
                // indirect블럭 생성
                int indirBlkno = newWriteBlockNum;
                InitInDirEntry(pBuf);
                DevWriteBlock(indirBlkno, pBuf);
                SetBlockBytemap(indirBlkno);
                ((FileSysInfo *)fileSysInfo)->numAllocBlocks++;
                ((FileSysInfo *)fileSysInfo)->numFreeBlocks--;
                pInode->indirectBlockPtr = indirBlkno;

                // direct블럭 생성 후 저장
                newWriteBlockNum = GetFreeBlockNum();
                DevWriteBlock(newWriteBlockNum, writeItem[writeItemIdx]);
                SetBlockBytemap(newWriteBlockNum);
                PutIndirectBlockEntry(indirBlkno, 0, newWriteBlockNum);
                ((FileSysInfo *)fileSysInfo)->numAllocBlocks++;
                ((FileSysInfo *)fileSysInfo)->numFreeBlocks--;
                pInode->allocBlocks++;
                pInode->size += 512;
                PutInode(fileInode, pInode);
            } else {
                if (pInode->dirBlockPtr[logicalDirBlockIdx] > 0) {
                    DevWriteBlock(pInode->dirBlockPtr[logicalDirBlockIdx], writeItem[writeItemIdx]);
                } else {
                    DevWriteBlock(newWriteBlockNum, writeItem[writeItemIdx]);
                    SetBlockBytemap(newWriteBlockNum);
                    ((FileSysInfo *)fileSysInfo)->numAllocBlocks++;
                    ((FileSysInfo *)fileSysInfo)->numFreeBlocks--;
                    pInode->allocBlocks++;
                    pInode->dirBlockPtr[logicalDirBlockIdx] = newWriteBlockNum;
                    pInode->size += 512;
                    PutInode(fileInode, pInode);
                }
            }
        }
        pFileTable->pFile[pFileDescTable->pEntry[fileDesc].fileTableIndex].fileOffset += 512;
        DevWriteBlock(0, fileSysInfo);
        writeItemIdx++;
        free(pBuf);
        free(pInode);
        free(fileSysInfo);
        free(pDirEntry);
    }
    return length;
}

int ReadFile(int fileDesc, char *pBuffer, int length) {
    if (fileDesc == -1)
        return -1;
    int loopCnt = 0;
    int tmp = length / BLOCK_SIZE;
    char *readItem[tmp];
    while (tmp--) {
        readItem[loopCnt] = malloc(BLOCK_SIZE * sizeof(char));
        memset(readItem[loopCnt], 0, BLOCK_SIZE * sizeof(char));
        loopCnt++;
    }

    int readItemIdx = 0;
    while (loopCnt--) {
        int fileInode = pFileTable->pFile[pFileDescTable->pEntry[fileDesc].fileTableIndex].inodeNum;
        int fileOffset = pFileTable->pFile[pFileDescTable->pEntry[fileDesc].fileTableIndex].fileOffset;
        int logicalDirBlockIdx = fileOffset / BLOCK_SIZE;

        Inode *pInode = (Inode *)malloc(sizeof(Inode));
        memset(pInode, '\0', sizeof(Inode));

        GetInode(fileInode, pInode);

        if (logicalDirBlockIdx > 3) {
            logicalDirBlockIdx -= 4;
            int dirBlkno = GetIndirectBlockEntry(pInode->indirectBlockPtr, logicalDirBlockIdx);
            DevReadBlock(dirBlkno, readItem[readItemIdx]);
        } else {
            int dirBlkno = pInode->dirBlockPtr[logicalDirBlockIdx];
            DevReadBlock(dirBlkno, readItem[readItemIdx]);
        }

        pFileTable->pFile[pFileDescTable->pEntry[fileDesc].fileTableIndex].fileOffset += 512;
        // printf("%d번째 읽은 문자열: %s\n", readItemIdx, readItem[readItemIdx]);
        readItemIdx++;
        free(pInode);
    }

    tmp = length / BLOCK_SIZE;
    char *ans = malloc(sizeof(char) * length);
    for (int i = 0; i < tmp; i++) {
        // strcat(pBuffer, readItem[i]);
        memcpy(ans + BLOCK_SIZE * i, readItem[i], sizeof(char) * BLOCK_SIZE);
    }
    // printf("합친 문자열: %s\n", ans);
    strcpy(pBuffer, ans);
    free(ans);
    return 0;
}

int CloseFile(int fileDesc) {
    if (fileDesc < 0)
        return -1;
    int fileTableIdx = pFileDescTable->pEntry[fileDesc].fileTableIndex;
    pFileDescTable->pEntry[fileDesc].bUsed = 0;
    pFileDescTable->numUsedDescEntry--;

    pFileTable->numUsedFile--;
    pFileTable->pFile[fileTableIdx].bUsed = 0;

    return 0;
}

int RemoveFile(char *name) {
    int newFileInodeNum = GetFreeInodeNum();
    int rootInodeNum = 0;

    //경로관련
    char *pathList[512];
    int pathItemCnt = split(name, pathList);
    int cur = 0;
    // flag
    int isSameNameExist = 0;
    int newFileBlockLoc = -1;
    int newFileEntLoc = -1;
    int isFirstFreeEntry = 1;
    int isFileExist = 0;
    int parentInodeNum = -1;
    int existFileInode = -1;
    int freeFdEnt = -1;
    int fileTableIdx = -1;

    Inode *pInode = (Inode *)malloc(sizeof(Inode));
    memset(pInode, '\0', sizeof(Inode));
    Inode *checkInode = (Inode *)malloc(sizeof(Inode));
    memset(checkInode, '\0', sizeof(Inode));
    char *pBuf = malloc(BLOCK_SIZE * sizeof(char));
    memset(pBuf, '\0', BLOCK_SIZE * sizeof(char));
    char *fileSysInfo = malloc(BLOCK_SIZE * sizeof(char));
    memset(fileSysInfo, '\0', BLOCK_SIZE * sizeof(char));
    DevReadBlock(0, fileSysInfo);

    GetInode(rootInodeNum, pInode);

    DirEntry *pDirEntry = (DirEntry *)malloc(sizeof(DirEntry));
    memset(pDirEntry, '\0', sizeof(DirEntry));

    while (cur < pathItemCnt) {
        newFileBlockLoc = -1;
        newFileEntLoc = -1;
        isFirstFreeEntry = 1;

        for (int dirBlockPtrIdx = 0; dirBlockPtrIdx < pInode->allocBlocks; dirBlockPtrIdx++) {
            for (int dirEntIdx = 0; dirEntIdx < NUM_OF_DIRENT_PER_BLK; dirEntIdx++) {
                memset(pDirEntry, '\0', sizeof(DirEntry));
                if (GetDirEntry(pInode->dirBlockPtr[dirBlockPtrIdx], dirEntIdx, pDirEntry) == -1) {
                    continue;
                }

                if (strcmp(pDirEntry->name, pathList[cur]) == 0) {
                    GetInode(pDirEntry->inodeNum, checkInode);
                    if (checkInode->type == FILE_TYPE_FILE) {
                        if (cur == pathItemCnt - 1) {
                            isFileExist = 1;
                            existFileInode = pDirEntry->inodeNum;
                            isSameNameExist = 1;
                            pDirEntry->inodeNum = -1;
                            PutDirEntry(pInode->dirBlockPtr[dirBlockPtrIdx], dirEntIdx, pDirEntry);
                            break;
                        }
                    } else if (checkInode->type == FILE_TYPE_DIR) {
                        if (cur != pathItemCnt - 1) {
                            isSameNameExist = 1;
                            break;
                        }
                    }
                }

                if (strcmp(pDirEntry->name, ".") == 0 && (cur == pathItemCnt - 1)) {
                    parentInodeNum = pDirEntry->inodeNum;
                }
            }
            if (isSameNameExist) {
                if (isFileExist)
                    break;
                GetInode(pDirEntry->inodeNum, pInode);
                break;
            }
        }
        if (isSameNameExist) {
            isSameNameExist = 0;
            cur++;
            continue;
        } else {
            if (pInode->indirectBlockPtr < 1) {
                if (cur != pathItemCnt - 1) {
                    free(pBuf);
                    free(pInode);
                    free(pDirEntry);
                    free(checkInode);
                    return -1;
                }
                cur++;
            } else {
                int dirBlockNum = -1;
                for (int dirBlockIdx = 0; dirBlockIdx < BLOCK_SIZE / sizeof(int); dirBlockIdx) {
                    dirBlockNum = GetIndirectBlockEntry(pInode->indirectBlockPtr, dirBlockIdx);
                    if (dirBlockNum < 1)
                        continue;

                    for (int dirEntIdx = 0; dirEntIdx < NUM_OF_DIRENT_PER_BLK; dirEntIdx++) {
                        if (GetDirEntry(dirBlockNum, dirEntIdx, pDirEntry) == -1) {
                            continue;
                        }
                        if (strcmp(pDirEntry->name, pathList[cur]) == 0) {
                            GetInode(pDirEntry->inodeNum, checkInode);
                            if (checkInode->type == FILE_TYPE_FILE) {
                                if (cur == pathItemCnt - 1) {
                                    isFileExist = 1;
                                    existFileInode = pDirEntry->inodeNum;
                                    isSameNameExist = 1;
                                    pDirEntry->inodeNum = -1;
                                    PutDirEntry(dirBlockNum, dirEntIdx, pDirEntry);
                                    break;
                                }
                            } else if (checkInode->type == FILE_TYPE_DIR) {
                                if (cur != pathItemCnt - 1) {
                                    isSameNameExist = 1;
                                    break;
                                }
                            }
                        }
                    }
                    if (isSameNameExist) {
                        if (isFileExist)
                            break;
                        GetInode(pDirEntry->inodeNum, pInode);
                        break;
                    }
                }
                if (isSameNameExist) {
                    isSameNameExist = 0;
                    cur++;
                    continue;
                } else if (cur != pathItemCnt - 1)
                    return -1;
                else
                    cur++;
            }
        }
    }

    if (!isFileExist) {
        free(pInode);
        free(pBuf);
        free(checkInode);
        free(pDirEntry);
        return -1;
    }

    GetInode(existFileInode, pInode);
    int dblockno;
    for (int i = 0; i < NUM_OF_DIRECT_BLOCK_PTR; i++) {
        dblockno = pInode->dirBlockPtr[i];
        if (dblockno > 0) {
            ResetBlockBytemap(dblockno);
            pInode->dirBlockPtr[i] = -1;
            ((FileSysInfo *)fileSysInfo)->numAllocBlocks--;
            ((FileSysInfo *)fileSysInfo)->numFreeBlocks++;
        }
    }
    if (pInode->indirectBlockPtr > 0) {
        int idblockno;
        idblockno = pInode->indirectBlockPtr;
        for (int i = 0; i < 128; i++) {
            dblockno = GetIndirectBlockEntry(idblockno, i);
            if (dblockno > 0) {
                ResetBlockBytemap(dblockno);
                PutIndirectBlockEntry(idblockno, i, -1);
                ((FileSysInfo *)fileSysInfo)->numAllocBlocks--;
                ((FileSysInfo *)fileSysInfo)->numFreeBlocks++;
            }
        }
        ResetBlockBytemap(idblockno);
        ((FileSysInfo *)fileSysInfo)->numAllocBlocks--;
        ((FileSysInfo *)fileSysInfo)->numFreeBlocks++;
    }
    pInode->allocBlocks = 0;
    pInode->indirectBlockPtr = -1;
    pInode->size = 0;
    PutInode(existFileInode, pInode);
    ResetInodeBytemap(existFileInode);
    ((FileSysInfo *)fileSysInfo)->numAllocInodes--;

    DevWriteBlock(0, fileSysInfo);

    GetInode(parentInodeNum, pInode);

    if (pInode->indirectBlockPtr > 0) {
        int inDirBlkno = pInode->indirectBlockPtr;
        int dirBlkno = -1;
        int canDeleteIndirBlk = 1;
        for (int i = 0; i < 128; i++) {
            dirBlkno = GetIndirectBlockEntry(inDirBlkno, i);
            if (dirBlkno > 0) {
                int canDeleteBlk = 1;
                for (int dirEntIdx = 0; dirEntIdx < NUM_OF_DIRENT_PER_BLK; dirEntIdx++) {
                    if (GetDirEntry(dirBlkno, dirEntIdx, pDirEntry) != -1) {
                        canDeleteBlk = 0;
                        canDeleteIndirBlk = 0;
                        break;
                    }
                }
                if (canDeleteBlk) {
                    ResetBlockBytemap(dirBlkno);
                    ((FileSysInfo *)fileSysInfo)->numAllocBlocks--;
                    ((FileSysInfo *)fileSysInfo)->numFreeBlocks++;
                    DevWriteBlock(0, fileSysInfo);
                    PutIndirectBlockEntry(inDirBlkno, i, -1);
                    pInode->allocBlocks--;
                    pInode->size -= 512;
                    PutInode(parentInodeNum, pInode);
                }
            }
        }
        if (canDeleteIndirBlk) {
            ResetBlockBytemap(inDirBlkno);
            ((FileSysInfo *)fileSysInfo)->numAllocBlocks--;
            ((FileSysInfo *)fileSysInfo)->numFreeBlocks++;
            DevWriteBlock(0, fileSysInfo);
            pInode->indirectBlockPtr = -1;
            PutInode(parentInodeNum, pInode);
        }
    } else {
        DirEntry saveDirEntry[32];
        int cnt = 0;
        int cntDirEntry = 0;
        for (int i = 0; i < NUM_OF_DIRECT_BLOCK_PTR; i++) {
            int dirBlkno = pInode->dirBlockPtr[i];
            if (dirBlkno > 0) {
                cnt++;
                for (int dirEntIdx = 0; dirEntIdx < NUM_OF_DIRENT_PER_BLK; dirEntIdx++) {
                    if (GetDirEntry(dirBlkno, dirEntIdx, pDirEntry) != -1) {
                        saveDirEntry[cntDirEntry] = *pDirEntry;
                        cntDirEntry++;
                    }
                }
                char *tp = malloc(sizeof(char) * BLOCK_SIZE);
                InitDirEntry(tp);
                DevWriteBlock(dirBlkno, tp);
                free(tp);
            }
        }

        int dIdx = 0;
        while (cntDirEntry > 0) {
            for (int i = 0; i < 8 && cntDirEntry > 0; i++) {
                PutDirEntry(pInode->dirBlockPtr[dIdx], i, &saveDirEntry[dIdx * 8 + i]);
                cntDirEntry--;
            }
            dIdx++;
        }
        for (int i = dIdx; i < NUM_OF_DIRECT_BLOCK_PTR; i++) {
            int dirBlkno = pInode->dirBlockPtr[i];
            if (dirBlkno > 0) {
                ResetBlockBytemap(dirBlkno);
                ((FileSysInfo *)fileSysInfo)->numAllocBlocks--;
                ((FileSysInfo *)fileSysInfo)->numFreeBlocks++;
                DevWriteBlock(0, fileSysInfo);
                pInode->dirBlockPtr[i] = 0;
                pInode->allocBlocks--;
                pInode->size -= 512;
            }
        }
        PutInode(parentInodeNum, pInode);
    }
    /*printf("blocks: %d\n rootInodeNum: %d\n diskCapacity: %d\n numAllocBlocks: %d\n numAllocInodes: %d\n numFreeBlocks: %d\n\n", ((FileSysInfo *)fileSysInfo)->blocks, ((FileSysInfo *)fileSysInfo)->rootInodeNum, ((FileSysInfo *)fileSysInfo)->diskCapacity, ((FileSysInfo
       *)fileSysInfo)->numAllocBlocks,
           ((FileSysInfo *)fileSysInfo)->numAllocInodes, ((FileSysInfo *)fileSysInfo)->numFreeBlocks);*/
    free(pInode);
    free(pBuf);
    free(checkInode);
    free(pDirEntry);
    free(fileSysInfo);

    return 0;
}

int MakeDirectory(char *name) {
    int newDirBlockNum = GetFreeBlockNum();
    int newDirInodeNum = GetFreeInodeNum();
    int rootInodeNum = 0;

    //경로관련
    char *pathList[512];
    int pathItemCnt = split(name, pathList);
    int cur = 0;
    // flag
    int isSameNameExist = 0;

    Inode *pInode = (Inode *)malloc(sizeof(Inode));
    memset(pInode, '\0', sizeof(Inode));
    Inode *checkInode = (Inode *)malloc(sizeof(Inode));
    memset(checkInode, '\0', sizeof(Inode));
    char *pBuf = malloc(BLOCK_SIZE * sizeof(char));
    memset(pBuf, '\0', BLOCK_SIZE * sizeof(char));
    char *fileSysInfo = malloc(BLOCK_SIZE * sizeof(char));
    memset(fileSysInfo, '\0', BLOCK_SIZE * sizeof(char));
    DevReadBlock(0, fileSysInfo);

    GetInode(rootInodeNum, pInode);

    DirEntry *pDirEntry = (DirEntry *)malloc(sizeof(DirEntry));
    memset(pDirEntry, '\0', sizeof(DirEntry));

    int newDirBlockLoc = -1;
    int newDirEntLoc = -1;
    int isFirstFreeEntry = 1;
    int parentInodeNum = -1;

    while (cur < pathItemCnt) {
        newDirBlockLoc = -1;
        newDirEntLoc = -1;
        isFirstFreeEntry = 1;

        for (int dirBlockPtrIdx = 0; dirBlockPtrIdx < NUM_OF_DIRECT_BLOCK_PTR; dirBlockPtrIdx++) {
            if (pInode->dirBlockPtr[dirBlockPtrIdx] < 1)
                continue;
            for (int dirEntIdx = 0; dirEntIdx < NUM_OF_DIRENT_PER_BLK; dirEntIdx++) {
                memset(pDirEntry, '\0', sizeof(DirEntry));
                if (GetDirEntry(pInode->dirBlockPtr[dirBlockPtrIdx], dirEntIdx, pDirEntry) == -1) {
                    if (isFirstFreeEntry) {
                        newDirBlockLoc = pInode->dirBlockPtr[dirBlockPtrIdx];
                        newDirEntLoc = dirEntIdx;
                        isFirstFreeEntry = 0;
                    }
                    continue;
                }

                if (strcmp(pDirEntry->name, pathList[cur]) == 0) {
                    GetInode(pDirEntry->inodeNum, checkInode);
                    if (checkInode->type == FILE_TYPE_DIR) {
                        if (cur == pathItemCnt - 1) {
                            free(pBuf);
                            free(pInode);
                            free(pDirEntry);
                            free(checkInode);
                            return -1;
                        }
                        isSameNameExist = 1;
                        break;
                    }
                }

                if (strcmp(pDirEntry->name, ".") == 0 && (cur == pathItemCnt - 1)) {
                    parentInodeNum = pDirEntry->inodeNum;
                }
            }
            if (isSameNameExist) {
                GetInode(pDirEntry->inodeNum, pInode);
                break;
            }
        }
        if (isSameNameExist) {
            isSameNameExist = 0;
            cur++;
            continue;
        } else {
            if (pInode->indirectBlockPtr < 1) {
                if (cur != pathItemCnt - 1) {
                    free(pBuf);
                    free(pInode);
                    free(pDirEntry);
                    free(checkInode);
                    return -1;
                }
                cur++;
            } else {
                int dirBlockNum = -1;
                for (int dirBlockIdx = 0; dirBlockIdx < BLOCK_SIZE / sizeof(int); dirBlockIdx++) {
                    dirBlockNum = GetIndirectBlockEntry(pInode->indirectBlockPtr, dirBlockIdx);
                    if (dirBlockNum < 1)
                        continue;

                    for (int dirEntIdx = 0; dirEntIdx < NUM_OF_DIRENT_PER_BLK; dirEntIdx++) {
                        if (GetDirEntry(dirBlockNum, dirEntIdx, pDirEntry) == -1)
                            if (isFirstFreeEntry) {
                                newDirBlockLoc = dirBlockNum;
                                newDirEntLoc = dirEntIdx;
                                isFirstFreeEntry = 0;
                            }
                        continue;

                        if (strcmp(pDirEntry->name, pathList[cur]) == 0) {
                            GetInode(pDirEntry->inodeNum, checkInode);
                            if (checkInode->type == FILE_TYPE_DIR) {
                                if (cur == pathItemCnt - 1) {
                                    free(pBuf);
                                    free(pInode);
                                    free(pDirEntry);
                                    free(checkInode);
                                    return -1;
                                }
                                isSameNameExist = 1;
                                break;
                            }
                        }
                    }
                    if (isSameNameExist) {
                        GetInode(pDirEntry->inodeNum, pInode);
                        break;
                    }
                }
                if (isSameNameExist) {
                    isSameNameExist = 0;
                    cur++;
                    continue;
                } else if (cur != pathItemCnt - 1)
                    return -1;
                else
                    cur++;
            }
        }
    }

    if (newDirEntLoc == -1 || newDirBlockLoc == -1) {
        newDirBlockLoc = newDirBlockNum;
        newDirEntLoc = 0;

        pInode->allocBlocks++;
        pInode->size += 512;
        PutInode(parentInodeNum, pInode);

        // dirBlock 생성 후 저장
        char *pblk = malloc(BLOCK_SIZE * sizeof(char));
        memset(pblk, '\0', BLOCK_SIZE * sizeof(char));
        InitDirEntry(pblk);
        DevWriteBlock(newDirBlockLoc, pblk);
        ((FileSysInfo *)fileSysInfo)->numAllocBlocks++;
        ((FileSysInfo *)fileSysInfo)->numFreeBlocks--;
        DevWriteBlock(0, fileSysInfo);

        free(pblk);
        SetBlockBytemap(newDirBlockLoc);

        // Inode에 생성한 블럭과 정보 저장

        // indirectBlockPtr에 저장
        if (pInode->indirectBlockPtr > 0) {
            for (int i = 0; i < 128; i++) {
                if (GetIndirectBlockEntry(pInode->indirectBlockPtr, i) == -1) {
                    PutIndirectBlockEntry(pInode->indirectBlockPtr, i, newDirBlockLoc);
                    break;
                }
            }
        } else {
            if (pInode->allocBlocks == 5) {
                char *pInDblk = malloc(BLOCK_SIZE * sizeof(char));
                memset(pInDblk, '\0', sizeof(BLOCK_SIZE * sizeof(char)));
                InitInDirEntry(pInDblk);
                int inDirBlkNo = GetFreeBlockNum();
                DevWriteBlock(inDirBlkNo, pInDblk);
                SetBlockBytemap(inDirBlkNo);
                ((FileSysInfo *)fileSysInfo)->numAllocBlocks++;
                ((FileSysInfo *)fileSysInfo)->numFreeBlocks--;
                DevWriteBlock(0, fileSysInfo);
                pInode->indirectBlockPtr = inDirBlkNo;
                PutInode(parentInodeNum, pInode);
                PutIndirectBlockEntry(pInode->indirectBlockPtr, 0, newDirBlockLoc);
            } else {
                // pInode->allocBlocks++;
                // pInode->size += 512;
                pInode->dirBlockPtr[pInode->allocBlocks - 1] = newDirBlockLoc;
                PutInode(parentInodeNum, pInode);
            }
        }
        newDirBlockNum = GetFreeBlockNum();
    }

    pDirEntry->inodeNum = newDirInodeNum;
    memcpy(pDirEntry->name, pathList[pathItemCnt - 1], strlen(pathList[pathItemCnt - 1]));
    PutDirEntry(newDirBlockLoc, newDirEntLoc, pDirEntry);

    InitDirEntry(pBuf);
    ((DirEntry *)pBuf)->inodeNum = newDirInodeNum;
    memcpy(((DirEntry *)pBuf)->name, ".", strlen("."));
    ((DirEntry *)pBuf + 1)->inodeNum = parentInodeNum;
    memcpy(((DirEntry *)pBuf + 1)->name, "..", strlen(".."));

    DevWriteBlock(newDirBlockNum, pBuf);

    GetInode(newDirInodeNum, pInode);
    pInode->allocBlocks = 1;
    pInode->size = 512;
    pInode->type = FILE_TYPE_DIR;
    pInode->dirBlockPtr[0] = newDirBlockNum;
    pInode->indirectBlockPtr = -1;
    PutInode(newDirInodeNum, pInode);

    SetInodeBytemap(newDirInodeNum);
    SetBlockBytemap(newDirBlockNum);

    ((FileSysInfo *)fileSysInfo)->numAllocBlocks++;
    ((FileSysInfo *)fileSysInfo)->numFreeBlocks--;
    ((FileSysInfo *)fileSysInfo)->numAllocInodes++;

    DevWriteBlock(0, fileSysInfo);

    /*printf("blocks: %d\n rootInodeNum: %d\n diskCapacity: %d\n numAllocBlocks: %d\n numAllocInodes: %d\n numFreeBlocks: %d\n\n", ((FileSysInfo *)fileSysInfo)->blocks, ((FileSysInfo *)fileSysInfo)->rootInodeNum, ((FileSysInfo *)fileSysInfo)->diskCapacity, ((FileSysInfo
       *)fileSysInfo)->numAllocBlocks,
           ((FileSysInfo *)fileSysInfo)->numAllocInodes, ((FileSysInfo *)fileSysInfo)->numFreeBlocks);*/

    free(pInode);
    free(pBuf);
    free(fileSysInfo);
    free(checkInode);
    return 0;
}

int RemoveDirectory(char *name) {
    int canRemove = 0;
    int rootInodeNum = 0;

    //경로관련
    char *pathList[512];
    int pathItemCnt = split(name, pathList);
    int cur = 0;
    // flag
    int isSameNameExist = 0;
    int targetDirInodeNo = -1;
    int parentInodeNum = -1;
    int isEntryExist = 0;
    Inode *pInode = (Inode *)malloc(sizeof(Inode));
    memset(pInode, '\0', sizeof(Inode));
    Inode *checkInode = (Inode *)malloc(sizeof(Inode));
    memset(checkInode, '\0', sizeof(Inode));
    char *pBuf = malloc(BLOCK_SIZE * sizeof(char));
    memset(pBuf, '\0', BLOCK_SIZE * sizeof(char));
    char *fileSysInfo = malloc(BLOCK_SIZE * sizeof(char));
    memset(fileSysInfo, '\0', BLOCK_SIZE * sizeof(char));
    DevReadBlock(0, fileSysInfo);

    GetInode(rootInodeNum, pInode);

    DirEntry saveDirEntry;
    int saveBlkno;
    int saveIndex;
    DirEntry *pDirEntry = (DirEntry *)malloc(sizeof(DirEntry));
    memset(pDirEntry, '\0', sizeof(DirEntry));

    while (cur < pathItemCnt) {
        for (int dirBlockPtrIdx = 0; dirBlockPtrIdx < pInode->allocBlocks; dirBlockPtrIdx++) {
            for (int dirEntIdx = 0; dirEntIdx < NUM_OF_DIRENT_PER_BLK; dirEntIdx++) {
                memset(pDirEntry, '\0', sizeof(DirEntry));
                if (GetDirEntry(pInode->dirBlockPtr[dirBlockPtrIdx], dirEntIdx, pDirEntry) == -1) {
                    continue;
                }
                if (strcmp(pDirEntry->name, pathList[cur]) == 0) {
                    GetInode(pDirEntry->inodeNum, checkInode);
                    if (checkInode->type == FILE_TYPE_DIR) {
                        if (cur == pathItemCnt - 1) {
                            saveBlkno = pInode->dirBlockPtr[dirBlockPtrIdx];
                            saveIndex = dirEntIdx;
                            saveDirEntry = *pDirEntry;
                            targetDirInodeNo = pDirEntry->inodeNum;
                            pDirEntry->inodeNum = -1;

                            PutDirEntry(pInode->dirBlockPtr[dirBlockPtrIdx], dirEntIdx, pDirEntry);
                        }
                        isSameNameExist = 1;
                        break;
                    }
                }

                if (strcmp(pDirEntry->name, ".") == 0 && (cur == pathItemCnt - 1)) {
                    parentInodeNum = pDirEntry->inodeNum;
                }
            }
            if (isSameNameExist) {
                GetInode(pDirEntry->inodeNum, pInode);
                break;
            }
        }
        if (isSameNameExist) {
            isSameNameExist = 0;
            cur++;
            continue;
        } else {
            if (pInode->indirectBlockPtr < 1) {
                if (cur != pathItemCnt - 1) {
                    free(pBuf);
                    free(pInode);
                    free(pDirEntry);
                    free(checkInode);
                    return -1;
                }
                cur++;
            } else {
                int dirBlockNum = -1;
                for (int dirBlockIdx = 0; dirBlockIdx < BLOCK_SIZE / sizeof(int); dirBlockIdx++) {
                    dirBlockNum = GetIndirectBlockEntry(pInode->indirectBlockPtr, dirBlockIdx);
                    if (dirBlockNum < 1)
                        continue;
                    for (int dirEntIdx = 0; dirEntIdx < NUM_OF_DIRENT_PER_BLK; dirEntIdx++) {
                        if (GetDirEntry(dirBlockNum, dirEntIdx, pDirEntry) == -1)
                            continue;
                        if (strcmp(pDirEntry->name, pathList[cur]) == 0) {
                            GetInode(pDirEntry->inodeNum, checkInode);
                            if (checkInode->type == FILE_TYPE_DIR) {
                                if (cur == pathItemCnt - 1) {
                                    targetDirInodeNo = pDirEntry->inodeNum;
                                    pDirEntry->inodeNum = -1;
                                    PutDirEntry(dirBlockNum, dirEntIdx, pDirEntry);
                                }
                                isSameNameExist = 1;
                                break;
                            }
                        }
                        if (strcmp(pDirEntry->name, "..") == 0 && (cur == pathItemCnt - 1)) {
                            parentInodeNum = pDirEntry->inodeNum;
                        }
                    }
                    if (isSameNameExist) {
                        GetInode(pDirEntry->inodeNum, pInode);
                        break;
                    }
                }
                if (isSameNameExist) {
                    isSameNameExist = 0;
                    cur++;
                    continue;
                } else if (cur != pathItemCnt - 1)
                    return -1;
                else
                    cur++;
            }
        }
    }

    if (targetDirInodeNo == -1) {
        free(pInode);
        free(checkInode);
        free(pBuf);
        free(fileSysInfo);
        free(pDirEntry);
        return -1;
    }

    int cnt = 0;

    GetInode(targetDirInodeNo, pInode);
    if (pInode->allocBlocks > 1) {
        PutDirEntry(saveBlkno, saveIndex, &saveDirEntry);
        free(pInode);
        free(checkInode);
        free(pBuf);
        free(fileSysInfo);
        free(pDirEntry);
        return -1;
    } else if (pInode->allocBlocks == 1) {
        int dirBlkno = pInode->dirBlockPtr[0];
        for (int i = 0; i < NUM_OF_DIRENT_PER_BLK; i++) {
            if (GetDirEntry(dirBlkno, 0, pDirEntry) != -1) {
                cnt++;
            }
        }
        if (cnt == 2) {
            PutDirEntry(saveBlkno, saveIndex, &saveDirEntry);
            free(pInode);
            free(checkInode);
            free(pBuf);
            free(fileSysInfo);
            free(pDirEntry);
            return -1;
        }
    }

    ResetBlockBytemap(pInode->dirBlockPtr[0]);
    ResetInodeBytemap(targetDirInodeNo);
    ((FileSysInfo *)fileSysInfo)->numAllocBlocks--;
    ((FileSysInfo *)fileSysInfo)->numFreeBlocks++;
    ((FileSysInfo *)fileSysInfo)->numAllocInodes--;
    DevWriteBlock(0, fileSysInfo);

    GetInode(parentInodeNum, pInode);

    if (pInode->indirectBlockPtr > 0) {
        int inDirBlkno = pInode->indirectBlockPtr;
        int dirBlkno = -1;
        int canDeleteIndirBlk = 1;
        for (int i = 0; i < 128; i++) {
            dirBlkno = GetIndirectBlockEntry(inDirBlkno, i);
            if (dirBlkno > 0) {
                int canDeleteBlk = 1;
                for (int dirEntIdx = 0; dirEntIdx < NUM_OF_DIRENT_PER_BLK; dirEntIdx++) {
                    if (GetDirEntry(dirBlkno, dirEntIdx, pDirEntry) != -1) {
                        canDeleteBlk = 0;
                        canDeleteIndirBlk = 0;
                        break;
                    }
                }
                if (canDeleteBlk) {
                    ResetBlockBytemap(dirBlkno);
                    ((FileSysInfo *)fileSysInfo)->numAllocBlocks--;
                    ((FileSysInfo *)fileSysInfo)->numFreeBlocks++;
                    DevWriteBlock(0, fileSysInfo);
                    PutIndirectBlockEntry(inDirBlkno, i, -1);
                    pInode->allocBlocks--;
                    pInode->size -= 512;
                    PutInode(parentInodeNum, pInode);
                }
            }
        }
        if (canDeleteIndirBlk) {
            ResetBlockBytemap(inDirBlkno);
            ((FileSysInfo *)fileSysInfo)->numAllocBlocks--;
            ((FileSysInfo *)fileSysInfo)->numFreeBlocks++;
            DevWriteBlock(0, fileSysInfo);
            pInode->indirectBlockPtr = -1;
            PutInode(parentInodeNum, pInode);
        }
    } else {
        for (int i = 0; i < NUM_OF_DIRECT_BLOCK_PTR; i++) {
            int dirBlkno = pInode->dirBlockPtr[i];
            if (dirBlkno > 0) {
                int canDeleteBlock = 1;
                for (int dirEntIdx = 0; dirEntIdx < NUM_OF_DIRENT_PER_BLK; dirEntIdx++) {
                    if (GetDirEntry(dirBlkno, dirEntIdx, pDirEntry) != -1) {
                        // PutDirEntry(saveBlkno, saveIndex, &saveDirEntry);
                        canDeleteBlock = 0;
                        break;
                    }
                }
                if (canDeleteBlock) {
                    ResetBlockBytemap(dirBlkno);
                    ((FileSysInfo *)fileSysInfo)->numAllocBlocks--;
                    ((FileSysInfo *)fileSysInfo)->numFreeBlocks++;
                    DevWriteBlock(0, fileSysInfo);
                    pInode->dirBlockPtr[i] = 0;
                    pInode->allocBlocks -= 1;
                    pInode->size -= 512;
                    PutInode(parentInodeNum, pInode);
                }
            }
        }
    }

    /*printf("blocks: %d\n rootInodeNum: %d\n diskCapacity: %d\n numAllocBlocks: %d\n numAllocInodes: %d\n numFreeBlocks: %d\n\n", ((FileSysInfo *)fileSysInfo)->blocks, ((FileSysInfo *)fileSysInfo)->rootInodeNum, ((FileSysInfo *)fileSysInfo)->diskCapacity, ((FileSysInfo
       *)fileSysInfo)->numAllocBlocks,
           ((FileSysInfo *)fileSysInfo)->numAllocInodes, ((FileSysInfo *)fileSysInfo)->numFreeBlocks);*/

    free(pInode);
    free(checkInode);
    free(pBuf);
    free(fileSysInfo);
    free(pDirEntry);
    return 0;
}

void CreateFileSystem(void) {
    FileSysInit();
    int rootBlockNum = GetFreeBlockNum();
    int rootInodeNum = GetFreeInodeNum();
    char *rootName = ".";

    if (rootInodeNum != 0) {
        pFileDescTable = malloc(sizeof(FileDescTable));
        memset(pFileDescTable, '\0', sizeof(FileDescTable));
        pFileDescTable->numUsedDescEntry = 0;
        for (int i = 0; i < DESC_ENTRY_NUM; i++) {
            pFileDescTable->pEntry[i].bUsed = 0;
        }

        pFileTable = malloc(sizeof(FileTable));
        memset(pFileTable, '\0', sizeof(FileTable));
        pFileTable->numUsedFile = 0;
        for (int i = 0; i < MAX_FILE_NUM; i++) {
            pFileTable->pFile[i].bUsed = 0;
        }
        return;
    }

    //버퍼 초기화
    char *pBuf = malloc(BLOCK_SIZE * sizeof(char));

    //디렉토리 엔트리 초기화
    InitDirEntry(pBuf);

    //루트 디렉토리 설정
    memcpy(((DirEntry *)pBuf)->name, rootName, strlen(rootName) + 1);
    ((DirEntry *)pBuf)->inodeNum = rootInodeNum;

    DevWriteBlock(rootBlockNum, pBuf);

    free(pBuf);

    pBuf = malloc(BLOCK_SIZE * sizeof(char));

    ((FileSysInfo *)pBuf)->blocks = 512;
    ((FileSysInfo *)pBuf)->rootInodeNum = rootInodeNum;
    ((FileSysInfo *)pBuf)->diskCapacity = 512 * 512;
    ((FileSysInfo *)pBuf)->numAllocBlocks = 1;
    ((FileSysInfo *)pBuf)->numFreeBlocks = BLOCK_SIZE - 11 - 1;
    ((FileSysInfo *)pBuf)->numAllocInodes = 1;
    ((FileSysInfo *)pBuf)->blockBytemapBlock = BLOCK_BYTEMAP_BLOCK_NUM;
    ((FileSysInfo *)pBuf)->inodeBytemapBlock = INODE_BYTEMAP_BLOCK_NUM;
    ((FileSysInfo *)pBuf)->inodeListBlock = INODELIST_BLOCK_FIRST;

    DevWriteBlock(FILESYS_INFO_BLOCK, pBuf);
    free(pBuf);

    SetBlockBytemap(rootBlockNum);
    SetInodeBytemap(rootInodeNum);

    Inode *pInode = (Inode *)malloc(sizeof(Inode));

    GetInode(rootInodeNum, pInode);

    pInode->allocBlocks = 1;
    pInode->size = 512;
    pInode->type = FILE_TYPE_DIR;
    pInode->dirBlockPtr[0] = rootBlockNum;
    // indirect은 초기화 어떻게 해야할지 고민해봐야함.
    pInode->indirectBlockPtr = -1;

    pFileDescTable = malloc(sizeof(FileDescTable));
    memset(pFileDescTable, '\0', sizeof(FileDescTable));
    pFileDescTable->numUsedDescEntry = 0;
    for (int i = 0; i < DESC_ENTRY_NUM; i++) {
        pFileDescTable->pEntry[i].bUsed = 0;
    }

    pFileTable = malloc(sizeof(FileTable));
    memset(pFileTable, '\0', sizeof(FileTable));
    pFileTable->numUsedFile = 0;
    for (int i = 0; i < MAX_FILE_NUM; i++) {
        pFileTable->pFile[i].bUsed = 0;
    }

    PutInode(rootInodeNum, pInode);
    free(pInode);
}
void OpenFileSystem(void) {
    DevOpenDisk();
    pFileDescTable = malloc(sizeof(FileDescTable));
    memset(pFileDescTable, '\0', sizeof(FileDescTable));
    pFileDescTable->numUsedDescEntry = 0;
    for (int i = 0; i < DESC_ENTRY_NUM; i++) {
        pFileDescTable->pEntry[i].bUsed = 0;
    }

    pFileTable = malloc(sizeof(FileTable));
    memset(pFileTable, '\0', sizeof(FileTable));
    pFileTable->numUsedFile = 0;
    for (int i = 0; i < MAX_FILE_NUM; i++) {
        pFileTable->pFile[i].bUsed = 0;
    }
}

void CloseFileSystem(void) {
    DevCloseDisk();
    free(pFileDescTable);
    free(pFileTable);
}

Directory *OpenDirectory(char *name) {
    Directory *directory = malloc(sizeof(Directory));

    int rootInodeNum = 0;
    if (strcmp(name, "/") == 0) {
        directory->inodeNum = rootInodeNum;
        return directory;
    }

    //경로관련
    char *pathList[512];
    int pathItemCnt = split(name, pathList);
    int cur = 0;
    // flag
    int isSameNameExist = 0;
    int isDirExist = 0;
    int existDirInode = -1;
    int freeFdEnt = -1;
    int fileTableIdx = -1;
    int parentInodeNum = -1;

    Inode *pInode = (Inode *)malloc(sizeof(Inode));
    memset(pInode, '\0', sizeof(Inode));
    Inode *checkInode = (Inode *)malloc(sizeof(Inode));
    memset(checkInode, '\0', sizeof(Inode));
    char *pBuf = malloc(BLOCK_SIZE * sizeof(char));
    memset(pBuf, '\0', BLOCK_SIZE * sizeof(char));
    char *fileSysInfo = malloc(BLOCK_SIZE * sizeof(char));
    memset(fileSysInfo, '\0', BLOCK_SIZE * sizeof(char));

    DevReadBlock(0, fileSysInfo);

    GetInode(rootInodeNum, pInode);

    DirEntry *pDirEntry = (DirEntry *)malloc(sizeof(DirEntry));
    memset(pDirEntry, '\0', sizeof(DirEntry));

    while (cur < pathItemCnt) {
        for (int dirBlockPtrIdx = 0; dirBlockPtrIdx < pInode->allocBlocks; dirBlockPtrIdx++) {
            for (int dirEntIdx = 0; dirEntIdx < NUM_OF_DIRENT_PER_BLK; dirEntIdx++) {
                memset(pDirEntry, '\0', sizeof(DirEntry));
                if (GetDirEntry(pInode->dirBlockPtr[dirBlockPtrIdx], dirEntIdx, pDirEntry) == -1) {
                    continue;
                }

                if (strcmp(pDirEntry->name, pathList[cur]) == 0) {
                    GetInode(pDirEntry->inodeNum, checkInode);
                    if (checkInode->type == FILE_TYPE_DIR) {
                        if (cur == pathItemCnt - 1) {
                            isDirExist = 1;
                            existDirInode = pDirEntry->inodeNum;
                        }
                        isSameNameExist = 1;
                        break;
                    }
                }

                if (strcmp(pDirEntry->name, ".") == 0 && (cur == pathItemCnt - 1)) {
                    parentInodeNum = pDirEntry->inodeNum;
                }
            }
            if (isSameNameExist) {
                if (isDirExist)
                    break;
                GetInode(pDirEntry->inodeNum, pInode);
                break;
            }
        }
        if (isSameNameExist) {
            isSameNameExist = 0;
            cur++;
            continue;
        } else {
            if (pInode->indirectBlockPtr < 1) {
                if (cur != pathItemCnt - 1) {
                    free(pBuf);
                    free(pInode);
                    free(pDirEntry);
                    free(checkInode);
                    return NULL;
                }
                cur++;
            } else {
                int dirBlockNum = -1;
                for (int dirBlockIdx = 0; dirBlockIdx < BLOCK_SIZE / sizeof(int); dirBlockIdx) {
                    dirBlockNum = GetIndirectBlockEntry(pInode->indirectBlockPtr, dirBlockIdx);
                    if (dirBlockNum < 1)
                        continue;

                    for (int dirEntIdx = 0; dirEntIdx < NUM_OF_DIRENT_PER_BLK; dirEntIdx++) {
                        if (GetDirEntry(dirBlockNum, dirEntIdx, pDirEntry) == -1) {
                            continue;
                        }
                        if (strcmp(pDirEntry->name, pathList[cur]) == 0) {
                            GetInode(pDirEntry->inodeNum, checkInode);
                            if (checkInode->type == FILE_TYPE_DIR) {
                                if (cur == pathItemCnt - 1) {
                                    isDirExist = 1;
                                    existDirInode = pDirEntry->inodeNum;
                                }
                                isSameNameExist = 1;
                                break;
                            }
                        }
                    }
                    if (isSameNameExist) {
                        if (isDirExist)
                            break;
                        GetInode(pDirEntry->inodeNum, pInode);
                        break;
                    }
                }
                if (isSameNameExist) {
                    isSameNameExist = 0;
                    cur++;
                    continue;
                } else if (cur != pathItemCnt - 1)
                    return NULL;
                else
                    cur++;
            }
        }
    }

    free(pInode);
    free(pBuf);
    free(pDirEntry);
    free(checkInode);

    if (existDirInode == -1) {
        return NULL;
    }
    directory->inodeNum = existDirInode;
    return directory;
}

FileInfo *ReadDirectory(Directory *pDir) {
    if (pDir == NULL) {
        return NULL;
    }
    int targetDirInode = pDir->inodeNum;
    int curDirEntryIdx = cntDirIdx;
    int curIdx = 0;
    int dirBlkno = -1;
    int isDirEntExist = 0;
    FileInfo *pFileInfo = malloc(sizeof(FileInfo));
    Inode *pInode = malloc(sizeof(Inode));
    Inode *targetInode = malloc(sizeof(Inode));
    DirEntry *pDirEntry = malloc(sizeof(DirEntry));

    memset(pFileInfo, '\0', sizeof(FileInfo));
    memset(pInode, '\0', sizeof(Inode));
    memset(targetInode, '\0', sizeof(Inode));
    memset(pDirEntry, '\0', sizeof(DirEntry));

    GetInode(targetDirInode, pInode);

    for (int i = 0; i < NUM_OF_DIRECT_BLOCK_PTR; i++) {
        dirBlkno = pInode->dirBlockPtr[i];
        if (dirBlkno > 0) {
            for (int dirEntIdx = 0; dirEntIdx < NUM_OF_DIRENT_PER_BLK; dirEntIdx++) {
                memset(pDirEntry, '\0', sizeof(DirEntry));
                if (GetDirEntry(dirBlkno, dirEntIdx, pDirEntry) != -1) {
                    if (curIdx == curDirEntryIdx) {
                        isDirEntExist = 1;
                        GetInode(pDirEntry->inodeNum, targetInode);
                        pFileInfo->filetype = targetInode->type;
                        pFileInfo->inodeNum = pDirEntry->inodeNum;
                        pFileInfo->numBlocks = targetInode->allocBlocks;
                        pFileInfo->size = targetInode->size;
                        memcpy(pFileInfo->name, pDirEntry->name, strlen(pDirEntry->name) + 1);
                        break;
                    }
                    curIdx++;
                }
            }
        }
        if (isDirEntExist)
            break;
    }
    if (!isDirEntExist && pInode->indirectBlockPtr > 0) {
        int inDirBlckNo = pInode->indirectBlockPtr;
        for (int i = 0; i < 128; i++) {
            dirBlkno = GetIndirectBlockEntry(inDirBlckNo, i);
            if (dirBlkno > 0) {
                for (int dirEntIdx = 0; dirEntIdx < NUM_OF_DIRENT_PER_BLK; dirEntIdx++) {
                    memset(pDirEntry, '\0', sizeof(DirEntry));
                    if (GetDirEntry(dirBlkno, dirEntIdx, pDirEntry) != -1) {
                        if (curIdx == curDirEntryIdx) {
                            isDirEntExist = 1;
                            GetInode(pDirEntry->inodeNum, targetInode);
                            pFileInfo->filetype = targetInode->type;
                            pFileInfo->inodeNum = pDirEntry->inodeNum;
                            pFileInfo->numBlocks = targetInode->allocBlocks;
                            pFileInfo->size = targetInode->size;
                            memcpy(pFileInfo->name, pDirEntry->name, strlen(pDirEntry->name) + 1);
                            break;
                        }
                        curIdx++;
                    }
                }
            }
            if (isDirEntExist)
                break;
        }
    }

    free(pInode);
    free(targetInode);
    free(pDirEntry);
    if (isDirEntExist) {
        cntDirIdx++;
        return pFileInfo;
    } else {
        cntDirIdx = 0;
        return NULL;
    }
}

int CloseDirectory(Directory *pDir) {
    free(pDir);
    return 0;
}
