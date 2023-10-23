/**
 * Do not submit your assignment with a main function in this file.
 * If you submit with a main function in this file, you will get a zero.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "debug.h"
#include "sfmm.h"
#include <errno.h>

void addToFreeList(sf_block*);
size_t getBlockSize(sf_block*);
sf_block* setHeaderAndFooter(sf_block*, size_t, int);
void allocateBlock(sf_block*, sf_block*);
int allocateNewPage();
sf_block* coalesce(sf_block*);
int ceilSize(float);
int checkAllocated(sf_block*);
int checkValidPointer(void*);


//add block into appropriate free list
void addToFreeList(sf_block* block) {
    //determine which M list block belongs to
    size_t blockM = ((block->header >> 5)<<5) / 32;
    int fibFirstNum = 1;
    int fibSecondNum = 2;
    for(int i = 0; i < NUM_FREE_LISTS; i++) {
        //get fibonacci of i
        int fibM;
        if(i == 0 || i == 1) {
            fibM = i+1;
        } else {
            fibM = fibFirstNum + fibSecondNum;
            fibFirstNum = fibSecondNum;
            fibSecondNum = fibM;
        }
        //insert block into doubly linked list
        if(blockM <= fibM || i == NUM_FREE_LISTS - 1) {
            sf_free_list_heads[i].body.links.next->body.links.prev = block;
            block->body.links.prev = &sf_free_list_heads[i];
            block->body.links.next = sf_free_list_heads[i].body.links.next;
            sf_free_list_heads[i].body.links.next = block;
            return;
        }
    }
}

//return block size
size_t getBlockSize(sf_block* block) {
    return ((block->header >> 5) << 5);
}

//set header and footer of block
sf_block* setHeaderAndFooter(sf_block* block, size_t size, int allocateStatus) {
    block->header = (sf_header)(size | allocateStatus);
    sf_footer* footer = (sf_footer*)((char*)((block)) + size - sizeof(sf_footer));
    *footer = (sf_footer)(block->header);

    if(allocateStatus == 0) {
        block->header = ((block->header >> 5) << 5);
        *footer = (sf_footer)(block->header);
    }
    return block;
}

//free the block from freelist for use
void allocateBlock(sf_block* block, sf_block* prevBlock) {
    prevBlock->body.links.next = block->body.links.next;
    block->body.links.next->body.links.prev = prevBlock;
    block->body.links.prev = NULL;
    block->body.links.next = NULL;
}

//free a page of new memory and add it to wilderness block
int allocateNewPage() {
    sf_block* prevEpilogue = sf_mem_end() - 8;
    sf_block* newPage = sf_mem_grow();
    if(newPage == NULL) {
        return 0;
    }
    setHeaderAndFooter(prevEpilogue, PAGE_SZ, 0);
    sf_block* newEpilogue = sf_mem_end() - 8;
    newEpilogue->header = 0x10;
    addToFreeList(prevEpilogue);
    addToFreeList(coalesce(prevEpilogue));
    return 1;
}

//coalesce block with neighboring blocks
sf_block* coalesce(sf_block *block) {
    sf_block* prevBlockFooter = (void*)block - 8;
    sf_block* prevBlock = (void*)block - getBlockSize(prevBlockFooter);
    int prevBlockAllocated = checkAllocated(prevBlock);
    sf_block* nextBlock = (void*)block + getBlockSize(block);
    int nextBlockAllocated = checkAllocated(nextBlock);

    sf_block* prevBlockInList = block->body.links.prev;
    //4 cases of coalescing
    if(prevBlockAllocated && nextBlockAllocated) {
        //both allocated, return block
        allocateBlock(block, prevBlockInList);
        return block;
    } else if(prevBlockAllocated == 0 && nextBlockAllocated == 1) {
        //only next allocated, coalesce block and prev
        size_t size = getBlockSize(block) + getBlockSize(prevBlock);
        setHeaderAndFooter(prevBlock, size, 0);
        allocateBlock(block, prevBlockInList);
        allocateBlock(prevBlock, prevBlock->body.links.prev);
        return prevBlock;
    } else if(prevBlockAllocated == 1 && nextBlockAllocated == 0) {
        //only prev allocated, coalesce block and next
        size_t size = getBlockSize(block) + getBlockSize(nextBlock);
        setHeaderAndFooter(block, size, 0);
        allocateBlock(block, prevBlockInList);
        allocateBlock(nextBlock, nextBlock->body.links.prev);
        return block;
    } else {
        //both not allocated, coalesce block and next and prev
        size_t size = getBlockSize(block) + getBlockSize(prevBlock) + getBlockSize(nextBlock);
        setHeaderAndFooter(prevBlock, size, 0);
        allocateBlock(block, prevBlockInList);
        allocateBlock(nextBlock, nextBlock->body.links.prev);
        allocateBlock(prevBlock, prevBlock->body.links.prev);
        return prevBlock;
    }
}

int checkAllocated(sf_block* block) {
    int allocated = 0x10;
    if((block->header & allocated) == 0x10) {
        return 1;
    }
    return 0;
}

int ceilSize(float num) {
    int integer = (int)num;
    if(num == (float)integer) {
        return integer;
    }
    return integer + 1;
}

void *sf_malloc(size_t size) {
    if(size == 0) {
        return NULL;
    }

    //First time calling malloc
    if(sf_mem_start() == sf_mem_end()) {
        sf_mem_grow();
        sf_block* prologue = (sf_block*)(sf_mem_start() + 24);
        setHeaderAndFooter(prologue, 32, THIS_BLOCK_ALLOCATED);

        sf_block* epilogue = sf_mem_end()-8;
        epilogue->header = 0x10;

        sf_block* remaining = sf_mem_start() + 56;
        setHeaderAndFooter(remaining, 1984, 0);

        //initalize free list heads
        for(int i = 0; i < NUM_FREE_LISTS; i++) {
            sf_free_list_heads[i].body.links.prev = &sf_free_list_heads[i];
            sf_free_list_heads[i].body.links.next = &sf_free_list_heads[i];
            setHeaderAndFooter(&sf_free_list_heads[i], 0, THIS_BLOCK_ALLOCATED);
        }

        //add freedpage to free list
        addToFreeList(remaining);
    }
    //Make size a multiple of 32
    //if size + header and footer < blocksize then make it blocksize
    //else make it multiple of 32
    size_t newSize = size;
    if(size + 16 <= 32) {
        newSize = 32;
    } else {
        newSize = (int)ceilSize((float)(size + 16) / (float)32) * 32;
    }

    //Search in freelist to find fit for newSize block
    sf_block* freedBlock;
    int freedBlockSuccess = 0;
    while(freedBlockSuccess != 1) {
        for(int i = 0; i < NUM_FREE_LISTS; i++) {
            sf_block* head = &sf_free_list_heads[i];
            sf_block* prevBlock = &sf_free_list_heads[i];
            sf_block* currentBlock = head->body.links.next;
            while(currentBlock != head) {
                if(newSize <= getBlockSize(currentBlock)) {
                    freedBlockSuccess = 1;
                    //If splitting result in splinter, allocate whole block
                    //Else split the block
                    if(getBlockSize(currentBlock) - newSize < 32) {
                        allocateBlock(currentBlock, prevBlock);
                        freedBlock = setHeaderAndFooter(currentBlock, newSize, THIS_BLOCK_ALLOCATED);
                    } else {
                        allocateBlock(currentBlock, prevBlock);
                        size_t currentBlockSize = getBlockSize(currentBlock);
                        freedBlock = setHeaderAndFooter(currentBlock, newSize, THIS_BLOCK_ALLOCATED);
                        sf_block* remainPortion = (void*)currentBlock + newSize;
                        setHeaderAndFooter(remainPortion, currentBlockSize - newSize, 0);
                        addToFreeList(remainPortion);
                    }
                    break;
                }
                prevBlock = currentBlock;
                currentBlock = currentBlock->body.links.next;
            }
            if(freedBlockSuccess) break;
        }
        //Freed a block successfully
        if(freedBlockSuccess) {
            break;
        }
        //No more memory to allocate
        if(allocateNewPage() == 0) {
            sf_errno = ENOMEM;
            return NULL;
        }
    }
    return freedBlock->body.payload;
}

int checkValidPointer(void* ptr) {
    if(ptr == NULL) {
        return 0;
    }
    if((uintptr_t)ptr % 32 != 0) {
        return 0;
    }
    sf_block* block = ptr - 8;
    size_t size = getBlockSize(block);
    if(size < 32 || size % 32 != 0) {
        return 0;
    }
    if((void*)&block < sf_mem_start()) {
        return 0;
    }
    sf_footer* footer = (sf_footer*)((char*)((block)) + size - sizeof(sf_footer));
    if((void*)footer > sf_mem_end()) {
        return 0;
    }
    if(checkAllocated(block) == 0) {
        return 0;
    }
    return 1;
}

void sf_free(void *pp) {
    //check if pp valid
    if(checkValidPointer(pp) == 0) {
        abort();
    }
    //free pp
    sf_block* block = pp - 8;
    size_t size = getBlockSize(block);
    setHeaderAndFooter(block, size, 0);
    addToFreeList(block);
    addToFreeList(coalesce(block));
    return;
}

void *sf_realloc(void *pp, size_t rsize) {
    //check valid args
    if(checkValidPointer(pp) == 0) {
        sf_errno = EINVAL;
        return NULL;
    }
    sf_block* origBlock = pp;
    if(rsize == 0) {
        free(origBlock);
        return NULL;
    }

    origBlock = pp - 8;
    size_t origSize = getBlockSize(origBlock);
    //realloc to smaller size
    if(origSize >= rsize) {
        //Splitting results in splinter
        size_t newSize = (int)ceilSize((float)(rsize + 16) / (float)32) * 32;
        if(origSize - newSize < 32) {
            return pp;
        }
        //Can split
        setHeaderAndFooter(origBlock, newSize, THIS_BLOCK_ALLOCATED);
        sf_block* remainingBlock = (void*)origBlock + newSize;
        setHeaderAndFooter(remainingBlock, origSize - newSize, 0);
        addToFreeList(remainingBlock);
        addToFreeList(coalesce(remainingBlock));
        return pp;
    } else {
        //realloc to bigger size
        sf_block* newBlock = sf_malloc(rsize);
        if(newBlock == NULL) {
            return NULL;
        }
        memcpy(newBlock, pp, origSize - 8);
        sf_free(pp);
        return newBlock;
    }
    return NULL;
}

void *sf_memalign(size_t size, size_t align) {
    //Check valid args
    if(size == 0) {
        return NULL;
    }
    int isPowerOfTwo = 0;
    if((align & (align - 1)) == 0) {
        isPowerOfTwo = 1;
    }
    if(align < 32 || isPowerOfTwo == 0) {
        sf_errno = EINVAL;
        return NULL;
    }

    size_t estimatedSize = size + align + 32 + 16;
    sf_block* allocatedBlock = sf_malloc(estimatedSize);
    //Address already aligned, split end if needed
    if((size_t)allocatedBlock % align == 0) {
        return sf_realloc(allocatedBlock, size);
    }
    //Address not aligned
    size_t alignedAddress = (size_t)ceilSize((float)((size_t)allocatedBlock) / (float)align) * align;
    setHeaderAndFooter((sf_block*)alignedAddress, size, THIS_BLOCK_ALLOCATED);
    setHeaderAndFooter(allocatedBlock, alignedAddress - (size_t)allocatedBlock, THIS_BLOCK_ALLOCATED);
    return (sf_block*)alignedAddress + 8;
    return NULL;
}
