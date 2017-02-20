#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <assert.h>
#include <sandbox/sandbox.h>
#include <inc/syscall.h>
#include <core/lwc_types.h>
#include "output.h"

#define COUNT 10000

int main(void)
{
	int i = -1;
	int *pointer = NULL;
	lwc_res_t result;
	void *address = (void*)0x0000005eff8000;
	void *shared = mmap(address, (size_t)(1 << 12), PROT_READ | PROT_WRITE, 
						MAP_ANONYMOUS, 0, 0);
	lwc_rg_struct specs[1];
	specs[0].start = (vm_addrptr)shared;
	specs[0].end = ((vm_addrptr) shared) + PGSIZE;
	specs[0].opt = LWC_SHARED;

	memset(shared, 0, PGSIZE);
	i = lwc_create(specs, 1, &result);

	if (i == 1) {
		result.caller = result.n_lwc;
	}

	if (i != 1 && i != 0) {
		TFAILURE("Something went wrong\n");
		return -1;
	}

increase:
	pointer = (int *) shared;
	*pointer += 1;
	lwc_switch(result.caller, NULL, &result);
	if (*pointer < COUNT)
		goto increase;

	printf("Value %d\n", *pointer);
	TSUCCESS("Done.\n");
	
	return 0;
}