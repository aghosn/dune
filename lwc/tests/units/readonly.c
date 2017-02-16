#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <assert.h>
#include <sandbox/sandbox.h>
#include <inc/syscall.h>
#include <core/lwc_types.h>
#include "output.h"

int main(void) {
    int i = -1;
    lwc_res_t result;
    void *address = (void*)0x0000005eff8000;
    void *ro = mmap(address, (size_t)(1<<12), PROT_READ | PROT_WRITE,
                    MAP_ANONYMOUS, 0, 0);
    
    printf("Readonly address: %p\n", ro);

    lwc_rg_struct specs[1];
    specs[0].start = (vm_addrptr)ro;
    specs[0].end = ((vm_addrptr) ro) + PGSIZE;
    specs[0].opt = LWC_RO;

    memset(ro, 43, PGSIZE);
    
    i = lwc_create(specs, 1, &result);
    if (i == 1) {
        memset(ro, 42, PGSIZE);
       
        lwc_switch(result.n_lwc, NULL, &result);
    } else if (i == 0) {
        
        char *reader = ro;
        while (reader < ((char*)ro) + PGSIZE) {
            if (*reader != 42) {
                TFAILURE("Read incorrect value.\n");
                return -1;
            }
            reader++;
        }
        
        reader = (char*) ro;
        *reader = 4;
        TFAILURE("Error, was able to write.\n");
    } else {
        TFAILURE("Error\n");
        return -1;
    }
    return 0;
}
