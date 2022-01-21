

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

#include "sbmem.h"

int main()
{
    
    sbmem_init(32768*8); 

    return (0); 
}
