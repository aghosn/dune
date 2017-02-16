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
	void *shared = mmap(address, (size_t)(1<<12), PROT_READ | PROT_WRITE,
		MAP_ANONYMOUS, 0, 0);

	lwc_rg_struct specs[1];
	specs[0].start = (vm_addrptr)shared;
	specs[0].end = ((vm_addrptr) shared) + PGSIZE;
	specs[0].opt = LWC_SHARED;

	i = lwc_create(specs, 1, &result);
	if (i == 1) {
		memset(shared, 42, PGSIZE-1);
		
		lwc_switch(result.n_lwc, NULL, &result);
		
		char *reader = shared;
		while(reader < ((char*)shared) + PGSIZE-1) {
			if (*reader != 44) {
				TFAILURE("Error in parent.\n");
				return -2;
			}
			reader++;
		}
		TSUCCESS("Done!\n");

	} else if (i == 0) {
		
		char *reader = shared;
		while (reader < ((char*)shared) + PGSIZE-1) {
			if (*reader != 42) {
				TFAILURE("Not the proper value.\n");
				return -1;
			} else {
				*reader = 44;
			}
			reader++;
		}
		
		lwc_switch(result.caller, NULL, &result);

	} else {
		TFAILURE("Could not create context.\n");
		return -1;
	}
	return 0;
}