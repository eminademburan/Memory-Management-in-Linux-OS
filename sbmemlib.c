#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <pthread.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#include <sys/time.h>
#include <semaphore.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>

// Define a name for shared memory and semaphores; 
char sharedmem_name[30] = "/sharedMemoryForProject3";
char semName[10] = "sharedSem";
char semOpCloseName[10] = "openCloseSem";

// Define global pointers for segment sizes and segment size pointer
void* start;
int totalSize;

// Define prototype of the function
void* sbmem_alloc (int size);

// Define global semaphore(s)
sem_t *sem_mutex;
sem_t *sem_openClose;


// Define your stuctures and variables. 
struct blockNode {
	
	int nextPtrButInt;
	int length;
	
};

struct sharedMemoryInfo {

	int freelists[11];	
	int size;
	int processes[10]; 	
	int numProcess;
	int totalFragmentation;
};

//finals
const int BLOCK_OVERHEAD = sizeof( struct blockNode );				
const int MEMINFO_OVERHEAD = sizeof( struct sharedMemoryInfo );

const int MINREQSIZE = 128;
const int MAXREQSIZE = 4096;

const int LOG_MIN_SEGMENT = 15;
const int LOG_MAX_SEGMENT = 18;


// Find the pointer of the buddy of a block with a given order
void* findBuddy( void* block, int order )
{	
	return (void*) ((((char*) block - (char*) start) ^ (1 << (order))) + (char*) start);	
}

// -Unused- Find the pointer of the buddy of a block with a given order if buddy has higher address value
void* findBuddyRight( void* block, int order )
{
	return (void*)((char*) block + (1 << order));
}

// Logarithmic function with base 2
int log2n( int n)
{
	return ( n > 1 ) ? 1 + log2n(n/2) : 0;
}

// Check whether a number is a power of two or not
int isPowerOfTwo( int n )
{
	if ( n > 1 )
		return ( n % 2 ) ? 0 : isPowerOfTwo(n/2);
	return 1;
}

// Initialize memory segment with given size
int sbmem_init(int segmentsize)
{
	// check if segment size is valid
	if ( !isPowerOfTwo(segmentsize) || log2n(segmentsize) < LOG_MIN_SEGMENT || log2n(segmentsize) > LOG_MAX_SEGMENT )
	{
		printf ("ERROR: sbmem init called with illegal segment size"); // remove all printfs when you are submitting
		return (0);
	}
		
    	struct stat sbuf;
    	void *shm_start;
  	int fd;
    
        //remove old shared segments and semaphores
	shm_unlink (sharedmem_name);  	
    	sem_unlink (semName);  	
    	sem_unlink (semOpCloseName);
    	
    	//initalize two semaphore with initial values are equal to 1
    	sem_mutex = sem_open( semName, O_RDWR | O_CREAT, 0660, 1);
    	sem_openClose = sem_open( semOpCloseName, O_RDWR | O_CREAT, 0660, 1);
    	
    	if( sem_mutex < 0)
    	{
    		perror("can not create semaphore_mutex \n");
    	}
    	
    	if( sem_openClose < 0)
    	{
    		perror("can not create semaphore_oepnClose \n");
    	}
    
    	//initalize a shared segments
    	fd = shm_open(sharedmem_name, O_RDWR | O_CREAT, 0660);
    	//give size to shared segment
    	ftruncate(fd, segmentsize);
    
    	if (fd < 0) {
		perror("can not create shared memory\n");
		exit (1); 

    	} 

    	
    	fstat(fd, &sbuf);
    	
    	//find the return address of the shared segment
    	shm_start = mmap(NULL, sbuf.st_size, 
			 PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
		
	//close the file descriptor	 
    	close(fd);

	//initlize global shared memory pointer 
	start = shm_start;

    
    	//initilaize first blocks overhead
    	struct blockNode firstBlock;
    	firstBlock.nextPtrButInt = -1;	// null but int
	firstBlock.length = segmentsize;	// overhead included
	
    
    	struct blockNode* blockNodePtr;
    
    	blockNodePtr = (struct blockNode*) shm_start;
    
    	//put first block overhead to start address of the shared memory
    	*blockNodePtr = firstBlock;
    	
    
    	//initialize the shared memory info
    	struct sharedMemoryInfo sharedMem;
    
    	for ( int i = 0; i < 12; i++ )
    		sharedMem.freelists[i] = -1;
    
	sharedMem.freelists[0] = 0;
 	sharedMem.size = segmentsize;
	sharedMem.numProcess = 0;	
	sharedMem.totalFragmentation = 0;
	
	//put the shared memory info to first block
	struct sharedMemoryInfo* smiPtr = (struct sharedMemoryInfo*) ((char*)shm_start + BLOCK_OVERHEAD);
	
	*smiPtr = sharedMem;
	
	void* shmPtr;
	
	
	//allocate space for shared memory info in shared segments
	if ( MEMINFO_OVERHEAD < MINREQSIZE)
		shmPtr = sbmem_alloc(MINREQSIZE);
	else
		shmPtr = sbmem_alloc(MEMINFO_OVERHEAD);
	
    	return (0);
}

int sbmem_remove()
{
	//remove shared memery and semaphores
    	shm_unlink (sharedmem_name);
    	shm_unlink (semOpCloseName);
    	sem_unlink (semName);
    	return (0); 
}


int sbmem_open()
{

	//initialize global semaphore values
	sem_openClose = sem_open( semOpCloseName, O_RDWR );
	    
	//lock the mutex variable
	sem_wait(sem_openClose);
	    
	sem_mutex = sem_open( semName, O_RDWR );
	 
	    
	int fd;
	fd = shm_open( sharedmem_name, O_RDWR , 0777 );

	void* addr;
	pid_t pid;
	struct stat sbuf;
	fstat(fd, &sbuf);

	    
	addr = mmap(NULL, sbuf.st_size, 
				 PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
				 
	start = addr;
	
	pid = getpid();
	     
	int exist = 0;
	    
	struct sharedMemoryInfo* info = (struct sharedMemoryInfo*) ((char*)start + BLOCK_OVERHEAD);
	
	//controll whether a process call sbmem_open twice by without using sbmem_close
	for( int i = 0; i < 10; i++)
	{
		if( info->processes[i] == (int) pid)
	    	{
	    		exist = 1;
	    	}
	}
	    
	//if a process doing first call update process number using shared segment 
	if( exist == 0 )
	{	    	
		//if there are more than 9 process exit the function
		if( info->numProcess >= 10)
	   	{
	    		sem_post(sem_openClose);
	    		return -1;
	    	}
	    	else
	    	{
			info->processes[info->numProcess] = (int) pid;
	    		info->numProcess = info->numProcess + 1;
	    		totalSize = info->size;
	    	}
	    	
	}			  
	
	//unlock the mutex variable    
	sem_post(sem_openClose);

	return (0);
    
}

//this method helps main allocate method to use sempahore values
void* sbmem_alloc_helper (int reqsize)
{
	int size = reqsize + BLOCK_OVERHEAD;
	
	void* shm_start = start;
			 
	struct sharedMemoryInfo* smiPtr = (struct sharedMemoryInfo*) ((char*)shm_start + BLOCK_OVERHEAD);
	
	
	int* freelists = (*smiPtr).freelists;
	
	// find order (j) and index (i)
	int j = log2n(size);
	
	if ( !isPowerOfTwo(size) )	// if size is not a power of two, then log2n returns 1 less from the value we want
		j++;
		
	int i = log2n((*smiPtr).size) - j;
	
	// i < 0 means reqsize is too large and i is invalid, there is no space for request
	if ( i < 0 )
	{
		printf("ERROR: not enough space\n");
		return NULL;
	}

	
	// If there is an available block of i, return block
	if ( freelists[i] != -1 )
	{
		struct blockNode* blockPtr = (struct blockNode*) ((char*)shm_start + freelists[i]);
		freelists[i] = (*blockPtr).nextPtrButInt;
		
		(*blockPtr).nextPtrButInt = -2;
		
		return (void*)((char*)blockPtr + BLOCK_OVERHEAD);
	}
	else	// Split larger blocks
	{
		
		struct blockNode *blockPtr, *buddyPtr;
		blockPtr = (char*) sbmem_alloc_helper( 2 * reqsize + BLOCK_OVERHEAD ) - BLOCK_OVERHEAD;	// ( + BLOCK_OVERHEAD ) is to have the double of the previous size, (2*reqsize + overhead) + (overhead) = 2*(reqsize + overhead) = 2*size
		
		
		if ( blockPtr != NULL )
		{
			buddyPtr = (struct blockNode*) findBuddy( blockPtr, j );
			
			struct blockNode buddy;
			buddy.length = (1 << j);
			buddy.nextPtrButInt = -1;
			*buddyPtr = buddy;
			
			freelists[i] = (int) buddyPtr - (int) shm_start;	// freelists[i] was -1, so no loss of nodes
		}
		
		(*blockPtr).nextPtrButInt = -2;
		(*blockPtr).length = (1 << j);
		
		return (void*)((char*)blockPtr + BLOCK_OVERHEAD);
	}
}

void* sbmem_alloc (int reqsize)
{
	
	
	if ( reqsize < MINREQSIZE || reqsize > MAXREQSIZE )
	{
		printf("ERROR: invalid request size %d\n",reqsize);
		return(NULL);
	}
	
	// lock the mutex variable
	sem_wait(sem_mutex);
	
	void* returnValue = sbmem_alloc_helper(reqsize);
	
	
	// update total fragmentation
	if ( returnValue != NULL )
	{
		struct sharedMemoryInfo* smiPtr = (struct sharedMemoryInfo*) ((char*)start + BLOCK_OVERHEAD);
		int size = reqsize + BLOCK_OVERHEAD;
		
		// find order (j)
		int j = log2n(size);
		if ( !isPowerOfTwo(size) )	// if size is not a power of two, then log2n returns 1 less from the value we want
			j++;
		
		(*smiPtr).totalFragmentation += ((1 << j) - reqsize);
	}
	
	// unlock mutex variable
	sem_post(sem_mutex);
	
	return returnValue;
}




void sbmem_free_helper (void *ptr)
{
	struct blockNode* blockPtr = (struct blockNode*)((char*) ptr - BLOCK_OVERHEAD);
	
	int length = (*blockPtr).length;
	
	struct sharedMemoryInfo* smiPtr = (struct sharedMemoryInfo*) ((char*)start + BLOCK_OVERHEAD);
	int* freelists = (*smiPtr).freelists;

	// find order (j) and index (i)
	int j = log2n(length);
	int i = log2n((*smiPtr).size) - j;
	
	struct blockNode* buddyPtr = findBuddy(blockPtr,j);
	
	int blockReplacement = (int) blockPtr - (int) start;
	
	if ( (*buddyPtr).length != length )	// buddy is split
	{
		// add to freelists[i]
		int firstVal = freelists[i];
		freelists[i] = blockReplacement;
		(*blockPtr).nextPtrButInt = firstVal;
	}
	else if ( (*buddyPtr).nextPtrButInt == -2 )	// buddy is allocated
	{
		// add to freelists[i], same with above
		int firstVal = freelists[i];
		freelists[i] = blockReplacement;
		(*blockPtr).nextPtrButInt = firstVal;
	}
	else	// buddy is free, merge
	{
		// remove buddy from fl
		int nextOfBuddy = (*buddyPtr).nextPtrButInt;
		
		int buddyReplacement = (int) buddyPtr - (int) start;
		
		int curVal = freelists[i];
		
		struct blockNode* curBlockPtr;
		
		if ( curVal == buddyReplacement )
		{
			curBlockPtr = (struct blockNode*)((char*)start + curVal);
			freelists[i] = nextOfBuddy;
		}
		else
		{
		
			while ( curVal >= 0 )	// curVal != -1
			{
				curBlockPtr = (struct blockNode*)((char*)start + curVal);
				curVal = (*curBlockPtr).nextPtrButInt;
			
				if ( curVal == buddyReplacement )
				{
					(*curBlockPtr).nextPtrButInt = nextOfBuddy;
					break;
				}
			}
			
		}
		
		// set parent recursively
		struct blockNode* parentPtr;
		int dif = (int)blockPtr - (int)buddyPtr;	// blockReplacement - buddyReplacement is also ok
		
		if ( dif < 0 )	// buddy is on the right
		{
			parentPtr = blockPtr;
		}
		else	// buddy is on the left
		{
			parentPtr = buddyPtr;
		}
		
		(*parentPtr).length = 2 * length;
		
		sbmem_free_helper((void*)((char*) parentPtr + BLOCK_OVERHEAD));
				
	}
}

void sbmem_free (void *ptr)
{
	
	//lock the mutex variable
	sem_wait(sem_mutex);
	
	sbmem_free_helper(ptr);
	
	//unlock the mutex variable
	sem_post(sem_mutex);
}



int sbmem_close()
{

	//lock the mutex semaphore
	sem_wait(sem_openClose);
    
    	//find the shared memory info
	struct sharedMemoryInfo* info = (struct sharedMemoryInfo*) ((char*)start + BLOCK_OVERHEAD);
    
	pid_t pid;
	pid = getpid();
	
	int index = -1;
	int exist = 0;
    
    	//control whether this process could use shared memory
	for( int i = 0; i < 10; i++)
	{
		if( info->processes[i] == (int) pid)
		{
			exist = 1;
			index = i;
    		}
    	}
    
    	//if process exists in process table, delete it
	if( exist )
	{
		info->processes[index] = info->processes[info->numProcess - 1];
		info->numProcess = info->numProcess -1 ;
	}	
	else
	{
		//unlock the mutex variable
		sem_post(sem_openClose);
		return -1;
	}
     
     	//unlock the mutex variable
	sem_post(sem_openClose);
    
	return (0); 
}
 
void sbmem_print()
{
	struct sharedMemoryInfo* smiPtr = (struct sharedMemoryInfo*) ((char*)start + BLOCK_OVERHEAD);
	
	printf("Total Fragm: %d\n",(*smiPtr).totalFragmentation);
	

	struct blockNode* curBlockPtr;
	int curVal;
		
	for ( int m = 0; m < 11; m++)
	{
		printf("fl[%d]: ", m );
		
		curVal = (*smiPtr).freelists[m];
		
		while (curVal >= 0)
		{
			curBlockPtr = (struct blockNode*)((char*)start + curVal);
			printf("%d, ",curVal);
			curVal = (*curBlockPtr).nextPtrButInt;
		}
		
		printf("%d ||| ",curVal);
	}
	
	printf("\n");
}
