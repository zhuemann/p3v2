///////////////////////////////////////////////////////////////////////////////
//
// Copyright 2019-2020 Jim Skrentny
// Posting or sharing this file is prohibited, including any changes/additions.
//
///////////////////////////////////////////////////////////////////////////////
 
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <stdio.h>
#include <string.h>
#include "heapAlloc.h"
 
/*
 * This structure serves as the header for each allocated and free block.
 * It also serves as the footer for each free block but only containing size.
 */
typedef struct blockHeader {           
    int size_status;
    /*
    * Size of the block is always a multiple of 8.
    * Size is stored in all block headers and free block footers.
    *
    * Status is stored only in headers using the two least significant bits.
    *   Bit0 => least significant bit, last bit
    *   Bit0 == 0 => free block
    *   Bit0 == 1 => allocated block
    *
    *   Bit1 => second last bit 
    *   Bit1 == 0 => previous block is free
    *   Bit1 == 1 => previous block is allocated
    * 
    * End Mark: 
    *  The end of the available memory is indicated using a size_status of 1.
    * 
    * Examples:
    * 
    * 1. Allocated block of size 24 bytes:
    *    Header:
    *      If the previous block is allocated, size_status should be 27
    *      If the previous block is free, size_status should be 25
    * 
    * 2. Free block of size 24 bytes:
    *    Header:
    *      If the previous block is allocated, size_status should be 26
    *      If the previous block is free, size_status should be 24
    *    Footer:
    *      size_status should be 24
    */
} blockHeader;         

/* Global variable - DO NOT CHANGE. It should always point to the first block,
 * i.e., the block at the lowest address.
 */
blockHeader *heapStart = NULL;     

/* Size of heap allocation padded to round to nearest page size.
 */
int allocsize;

/*
 * Additional global variables may be added as needed below
 */
blockHeader *lastAllocMade = NULL;
 
/* 
 * Function for allocating 'size' bytes of heap memory.
 * Argument size: requested size for the payload
 * Returns address of allocated block on success.
 * Returns NULL on failure.
 * This function should:
 * - Check size - Return NULL if not positive or if larger than heap space.
 * - Determine block size rounding up to a multiple of 8 and possibly adding padding as a result.
 * - Use NEXT-FIT PLACEMENT POLICY to chose a free block
 * - Use SPLITTING to divide the chosen free block into two if it is too large.
 * - Update header(s) and footer as needed.
 * Tips: Be careful with pointer arithmetic and scale factors.
 */
void* allocHeap(int size) {     
    //TODO: Your code goes in here.
    //if size is a negative number return null
    if (size < 0) {
        return NULL;
    }
    //if the size is larger than allecated space return null
    if (size > allocsize) {
        return NULL;
    }
    if (lastAllocMade == NULL) {
	lastAllocMade = heapStart;
    }
    //gets the pointer for the last block possible
    blockHeader *memoryEnd =(void*)heapStart + allocsize;
    
   
    //Makes sure the block is correctly sized to multiple of 8
    int remainder = size % 8;
    int padding = 8 - remainder;
    int paddedSize = size;
    if (remainder != 0) {
        paddedSize = size + padding;
    }
    
    //while you can't find a free block keep going through the blocks looking at headers till you
    //find a free block you can use that also large enough to fit everything
    blockHeader *currentAllocatedBlock = lastAllocMade;
    // while the current block is filled or to small, look for next block 
    int counter = 0;
    while( ( (currentAllocatedBlock->size_status & 1) != 0 ) || ( (currentAllocatedBlock->size_status/8)*8 < paddedSize ) )  {
    	
	//if we have already been through the heap twice return null	
 	if( counter > 2) {
		return NULL;
	}

	int takenSize = currentAllocatedBlock->size_status / 8;
        takenSize = takenSize * 8;

        currentAllocatedBlock = (blockHeader*)((void*)currentAllocatedBlock + takenSize);
	//checks to see if we have been through the space if we have change counter and go back to beginning
        if (( (void*)currentAllocatedBlock + takenSize) > (void*)memoryEnd) {

           currentAllocatedBlock = NULL;
	   counter++;
	   currentAllocatedBlock = heapStart;
        }
    }

    //the block that gets past the last while look will aways be free and large enough to allocate in memeory 
    blockHeader *freeBlock = currentAllocatedBlock;

    //Here we check one more time that it is in fact free but it should always be the case
    if ( (freeBlock->size_status & 1) == 0 ) {
	//gets the total p bit of the free block
        int freeSize = freeBlock->size_status / 8;
        freeSize = freeSize * 8;
	blockHeader *nextBlockHeader = (void*)freeBlock + (paddedSize);
	//if the next block is free we need to split the current space into a free block and filled block
	if( ( nextBlockHeader->size_status & 1) == 0) {
            //create and change footer first of the newly created free block
            blockHeader *footer = (blockHeader*) ((void*)freeBlock + freeSize - 4);

	    //give the footer the correct size, its a bits will be /zerozero
            footer->size_status = freeSize - paddedSize;
	
            blockHeader *newFreeHeader = ((void*)freeBlock + paddedSize); //check if block behind me is allocated
        
            newFreeHeader->size_status = freeBlock->size_status - paddedSize; //changes the p bit of the new free header
	    //freeBlock->size_status = freeBlock->size_status + 1;
        }

	//if the next block is taken we just need to change the a bits of the next block
        if( ( nextBlockHeader->size_status & 1) == 1) {

	    nextBlockHeader->size_status = ((nextBlockHeader->size_status/8)*8) + 3;

	}
	
        //change the header so save off this block
        freeBlock->size_status = paddedSize + 3;
	
	//update the last allocmade to be the one you are currently making
        lastAllocMade = freeBlock;
        return ((void*)freeBlock) + 4;
    }

    return NULL;
} 
 
/* 
 * Function for freeing up a previously allocated block.
 * Argument ptr: address of the block to be freed up.
 * Returns 0 on success.
 * Returns -1 on failure.
 * This function should:
 * - Return -1 if ptr is NULL.
 * - Return -1 if ptr is not a multiple of 8.
 * - Return -1 if ptr is outside of the heap space.
 * - Return -1 if ptr block is already freed.
 * - USE IMMEDIATE COALESCING if one or both of the adjacent neighbors are free.
 * - Update header(s) and footer as needed.
 */                    
int freeHeap(void *ptr) {    
    //makes sure the pointer to be freed is not null
    if (ptr == NULL) {
	return -1;
    }
    //make sure the pointer to be freed is aligned
    if ((int)ptr % 8 != 0) {
        return -1;
    }
    //makes sure the pointer is insdie the memory range
    if ((void*)ptr < (void*)heapStart) { //maybe heapstart +4
        return -1;
    }

    //gets the pointer for the last block possible
    blockHeader *memoryEnd =(void*)heapStart + allocsize;
    //makes sure the pointer is insdie the memory range
    if ((void*)ptr > (void*)memoryEnd) {
        return -1;
    }

    //gets the block header of the ptr that is to be freed
    blockHeader *freeBlockHeader = (void*)ptr - 4;

    //pointer to be freed is already freeded return zero
    if ( ( freeBlockHeader->size_status & 1) == 0) {
	return -1;
    }
    
    int sizeOfNewFreeBlock = (freeBlockHeader->size_status / 8 ) * 8;

    blockHeader *nextBlockHeader = (void*)ptr + sizeOfNewFreeBlock - 4 ;

    //if the next block and previous block is already taken then fill the blick and update the footer   
    if ( ( ( ( nextBlockHeader->size_status%8 ) % 2) == 1) &&( (freeBlockHeader->size_status % 8) >= 2) ) {
	//creats next block footer
    	blockHeader *newFreeBlockFooter = (void*)ptr + sizeOfNewFreeBlock - 8;
	//upddates the new block size
    	newFreeBlockFooter->size_status = sizeOfNewFreeBlock;
	//update next block header
	nextBlockHeader->size_status = nextBlockHeader->size_status - 2 ; //mark next header a bits to no prvious block
	//update the new free blocks a bit
	freeBlockHeader->size_status = freeBlockHeader->size_status -1;  //change the currents block  a bits to be free

    }
    //keeps track of if a coalsce has already happened
    int hasBeenCoalescedBack = 0;

    //if the next block is free you need to coalecse backwards
    if ( ( ( nextBlockHeader->size_status % 8) % 2 ) == 0) {

	//gets the next blocks footer which will be the new combined footer	
	blockHeader *nextBlockFooter = (void*)nextBlockHeader + ((nextBlockHeader->size_status/8)*8) - 4;
	//updates the value of the footer for the new combined block	
	nextBlockFooter->size_status = nextBlockFooter->size_status + sizeOfNewFreeBlock ; 
	//changes the freeblockHeader to have the correct a bit for its own status
	freeBlockHeader->size_status = (nextBlockFooter->size_status/8)*8 + (freeBlockHeader->size_status % 8) - 1; 
	
	hasBeenCoalescedBack++;
    }
	
    //if the previous block is free you need to coalecse forwards 
    if ((freeBlockHeader->size_status % 8) < 2) {
	//change the next block header so its previous bit is zero or freed
	blockHeader *nextHeader = (void*)freeBlockHeader + (freeBlockHeader->size_status/8)*8;
        //if the next block is already filled change the a bits so it relfects the alst free
	if ( (nextHeader->size_status & 1) == 1) {
	    nextHeader->size_status = ((nextHeader->size_status/8)*8) + 1;
	}
	//gets the previous footer to get to prevous head
	blockHeader *previousFooter = (void*)freeBlockHeader - 4;
        //gets previous head so coalesce the two blocks
	blockHeader *previousHeader = (void*)previousFooter - previousFooter->size_status + 4;
        //updates the previous heads size status
	previousHeader->size_status = previousHeader->size_status + ((freeBlockHeader->size_status/8)*8);
        //creats the new footer for the coalesd blocks
	blockHeader *newFreeBlockFooter = (void*) previousHeader + ((previousHeader->size_status/8)*8) - 4;
	//upddates that new footer
	newFreeBlockFooter->size_status = ((previousHeader->size_status/8)*8);
        //if we already coalesed backwards we only want to update the size_status once
	if(hasBeenCoalescedBack == 0) {
	    freeBlockHeader->size_status = freeBlockHeader->size_status -1;  //change the currents block  a bits to be free
	}
    }
    
    return 0;
} 
 
/*
 * Function used to initialize the memory allocator.
 * Intended to be called ONLY once by a program.
 * Argument sizeOfRegion: the size of the heap space to be allocated.
 * Returns 0 on success.
 * Returns -1 on failure.
 */                    
int initHeap(int sizeOfRegion) {    
 
    static int allocated_once = 0; //prevent multiple initHeap calls
 
    int pagesize;  // page size
    int padsize;   // size of padding when heap size not a multiple of page size
    void* mmap_ptr; // pointer to memory mapped area
    int fd;

    blockHeader* endMark;
  
    if (0 != allocated_once) {
        fprintf(stderr, 
        "Error:mem.c: InitHeap has allocated space during a previous call\n");
        return -1;
    }
    if (sizeOfRegion <= 0) {
        fprintf(stderr, "Error:mem.c: Requested block size is not positive\n");
        return -1;
    }

    // Get the pagesize
    pagesize = getpagesize();

    // Calculate padsize as the padding required to round up sizeOfRegion 
    // to a multiple of pagesize
    padsize = sizeOfRegion % pagesize;
    padsize = (pagesize - padsize) % pagesize;

    allocsize = sizeOfRegion + padsize;

    // Using mmap to allocate memory
    fd = open("/dev/zero", O_RDWR);
    if (-1 == fd) {
        fprintf(stderr, "Error:mem.c: Cannot open /dev/zero\n");
        return -1;
    }
    mmap_ptr = mmap(NULL, allocsize, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
    if (MAP_FAILED == mmap_ptr) {
        fprintf(stderr, "Error:mem.c: mmap cannot allocate space\n");
        allocated_once = 0;
        return -1;
    }
  
    allocated_once = 1;

    // for double word alignment and end mark
    allocsize -= 8;

    // Initially there is only one big free block in the heap.
    // Skip first 4 bytes for double word alignment requirement.
    heapStart = (blockHeader*) mmap_ptr + 1;

    // Set the end mark
    endMark = (blockHeader*)((void*)heapStart + allocsize);
    endMark->size_status = 1;

    // Set size in header
    heapStart->size_status = allocsize;

    // Set p-bit as allocated in header
    // note a-bit left at 0 for free
    heapStart->size_status += 2;

    // Set the footer
    blockHeader *footer = (blockHeader*) ((void*)heapStart + allocsize - 4);
    footer->size_status = allocsize;
  
    return 0;
} 
                  
/* 
 * Function to be used for DEBUGGING to help you visualize your heap structure.
 * Prints out a list of all the blocks including this information:
 * No.      : serial number of the block 
 * Status   : free/used (allocated)
 * Prev     : status of previous block free/used (allocated)
 * t_Begin  : address of the first byte in the block (where the header starts) 
 * t_End    : address of the last byte in the block 
 * t_Size   : size of the block as stored in the block header
 */                     
void dumpMem() {     
 
    int counter;
    char status[5];
    char p_status[5];
    char *t_begin = NULL;
    char *t_end   = NULL;
    int t_size;

    blockHeader *current = heapStart;
    counter = 1;

    int used_size = 0;
    int free_size = 0;
    int is_used   = -1;

    fprintf(stdout, "************************************Block list***\
                    ********************************\n");
    fprintf(stdout, "No.\tStatus\tPrev\tt_Begin\t\tt_End\t\tt_Size\n");
    fprintf(stdout, "-------------------------------------------------\
                    --------------------------------\n");
    int breaker = 0; 
    while (current->size_status != 1) {
        t_begin = (char*)current;
        t_size = current->size_status;
    
        if (t_size & 1) {
            // LSB = 1 => used block
            strcpy(status, "used");
            is_used = 1;
            t_size = t_size - 1;
        } else {
            strcpy(status, "Free");
            is_used = 0;
        }

        if (t_size & 2) {
            strcpy(p_status, "used");
            t_size = t_size - 2;
        } else {
            strcpy(p_status, "Free");
        }

        if (is_used) 
            used_size += t_size;
        else 
            free_size += t_size;

        t_end = t_begin + t_size - 1;
    
        fprintf(stdout, "%d\t%s\t%s\t0x%08lx\t0x%08lx\t%d\n", counter, status, 
        p_status, (unsigned long int)t_begin, (unsigned long int)t_end, t_size);
    
        current = (blockHeader*)((char*)current + t_size);
        counter = counter + 1;
	breaker++;
	if (breaker > 15) {
	    printf("%s","exiting while loop");
	    current->size_status = 1;
	}
    }

    fprintf(stdout, "---------------------------------------------------\
                    ------------------------------\n");
    fprintf(stdout, "***************************************************\
                    ******************************\n");
    fprintf(stdout, "Total used size = %d\n", used_size);
    fprintf(stdout, "Total free size = %d\n", free_size);
    fprintf(stdout, "Total size = %d\n", used_size + free_size);
    fprintf(stdout, "***************************************************\
                    ******************************\n");
    fflush(stdout);

    return;  
} 
