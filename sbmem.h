


int sbmem_init(int segmentsize); 
int sbmem_remove(); 

void* sbmem_alloc_helper (int reqsize);
void sbmem_free_helper (void *ptr);

int sbmem_open(); 
void* sbmem_alloc (int size);
void sbmem_free(void *p);
int sbmem_close(); 



    
    
