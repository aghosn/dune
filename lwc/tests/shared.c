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
	void *shared = mmap(address, (size_t)(1<<12), PROT_READ | PROT_WRITE,
		MAP_ANONYMOUS, 0, 0);

	vm_addrptr start = (vm_addrptr) shared;
	vm_addrptr end = start + PGSIZE;

	lwc_rsrc_spec specs;
	lwc_rg_struct to_share = {start, end, LWC_SHARED};
	Q_INIT_HEAD(&(specs.ranges));
	Q_INIT_ELEM(&to_share, lk_rg);
	Q_INSERT_FRONT(&(specs.ranges), &(to_share), lk_rg);

	i = lwc_create(&specs, &result);
	if (i == 1) {
		printf("Parent accessing the shared space.\n");
		memset(shared, 42, PGSIZE-1);
		printf("The write is done.\n");
		lwc_switch(result.n_lwc, NULL, &result);
		printf("Parent checks that it can read from child.\n");
		char *reader = shared;
		while(reader < ((char*)shared) + PGSIZE-1) {
			if (*reader != 44) {
				printf("Error in parent.\n");
				return -2;
			}
			reader++;
		}
		printf("Parent done!\n");

	} else if (i == 0) {
		printf("Child accessing the shared space.\n");
		char *reader = shared;
		while (reader < ((char*)shared) + PGSIZE-1) {
			if (*reader != 42) {
				printf("Not the proper value.\n");
				return -1;
			} else {
				*reader = 44;
			}
			reader++;
		}
		printf("Child done verifying memory.\n");
		lwc_switch(result.caller, NULL, &result);

	} else {
		printf("Error\n");
		return -1;
	}
	return 0;
}