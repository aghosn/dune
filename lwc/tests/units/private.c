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
	void *private = mmap(address, (size_t) PGSIZE, PROT_READ | PROT_WRITE,
		MAP_ANONYMOUS, 0, 0);

	lwc_rg_struct specs[1];
	specs[0].start = (vm_addrptr)private;
	specs[0].end = ((vm_addrptr) private) + PGSIZE;
	specs[0].opt = LWC_UNMAP;


	i = lwc_create(specs, 1, &result);
	if (i == 1) {
		memset(private, 42, PGSIZE-1);
		lwc_switch(result.n_lwc, NULL, &result);
	} else if (i  == 0) {
		char *reader = private;
		//TODO: doesn't fail with proper error message but fails.
		while (reader < ((char*)private) + PGSIZE-1) {
			printf("%c", *reader);
			reader++;
		}

		TFAILURE("Error: Child was able to read!\n");
	}

	return 0;
}