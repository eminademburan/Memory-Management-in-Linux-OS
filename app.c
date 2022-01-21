#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include "sbmem.h"
#include <stdio.h>
#include <stdlib.h>
#include <semaphore.h>
#include <sys/mman.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "math.h"

void test(){
    
    int ret = sbmem_open();
    
    if (ret == -1)
        exit(1);
    
    srand(time(NULL));
    
    int rando;
    char* ptrs[1];
    
    for (int k = 0; k < 1; k++)
    {
    	rando = (rand() % (4096 - 128)) + 128;
    	ptrs[k] = sbmem_alloc(rando);
    }
    
    for (int k = 0; k < 1; k++)
    {
    	sbmem_free(ptrs[k]);
    }
    
    sbmem_close();
}

int main(){
    
    int numProc = 200;

    int pidlist[numProc];
    
    for(int i = 0; i < numProc; i++){
        
        pidlist[i] = fork();
        
        if(pidlist[i] == 0) {
            test();      
            exit(1);
            break;
        }
    }
    
    for ( int i = 0; i < numProc; i++)
    {
    	waitpid(pidlist[i],NULL,0);
    }
    
    
   
    return 0;
}
