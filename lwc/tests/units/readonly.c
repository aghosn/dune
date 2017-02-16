#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <assert.h>
#include <sandbox/sandbox.h>
#include <inc/syscall.h>
#include <core/lwc_types.h>

int main(void) {
    int i = -1;
    lwc_res_t result;
    void *address = (void*)0x0000005eff8000;
    void *ro = mmap(address, (size_t)(1<<12), PROT_READ | PROT_WRITE,
                    MAP_ANONYMOUS, 0, 0);
    
    printf("Readonly address: %p\n", ro);
    
    vm_addrptr start = (vm_addrptr) ro;
    vm_addrptr end = start + PGSIZE;
    
    lwc_rsrc_spec specs;
    lwc_rg_struct to_ro = {start, end, LWC_RO};
    Q_INIT_HEAD(&(specs.ranges));
    Q_INIT_ELEM(&to_ro, lk_rg);
    Q_INSERT_FRONT(&(specs.ranges), &(to_ro), lk_rg);

    memset(ro, 43, PGSIZE);
    
    i = lwc_create(&specs, &result);
    if (i == 1) {
        printf("Parent accessing the readonly space.\n");
        memset(ro, 42, PGSIZE);
        printf("The write is done.\n");
        lwc_switch(result.n_lwc, NULL, &result);
    } else if (i == 0) {
        printf("Child accessing the readonly space.\n");
        char *reader = ro;
        while (reader < ((char*)ro) + PGSIZE) {
            if (*reader != 42) {
                printf("Not the proper value %d.\n", (int)*reader);
                return -1;
            }
            reader++;
        }
        printf("Child read the content. Will try to overwrite.");
        fflush(stdout);
        reader = (char*) ro;
        *reader = 4;
        printf("Error, was able to write.\n");
        fflush(stdout);
    } else {
        printf("Error\n");
        return -1;
    }
    return 0;
}
