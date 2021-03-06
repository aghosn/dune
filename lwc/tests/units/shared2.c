#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <assert.h>
#include <sandbox/sandbox.h>
#include <inc/syscall.h>
#include <core/lwc_types.h>

int main(void)
{
	int i = -1;
	lwc_res_t result;
	void *address = (void*)0x0000005eff8000;
	void *shared = mmap(address, (size_t)(1 << 12), PROT_READ | PROT_WRITE, 
						MAP_ANONYMOUS, 0, 0);
	

	lwc_rg_struct specs[1];
	specs[0].start = (vm_addrptr)shared;
	specs[0].end = specs[0].start + PGSIZE;
	specs[0].opt = LWC_SHARED;


hey:
	i = lwc_create(specs, 1, &result);

	if (i == 1) {
		printf("Hello world from the parent.\n");
		printf("The result %p and new lwc %p, label %p, 0x%016lx\n", &result,
			result.n_lwc, &&hey,(unsigned long)shared);
		fflush(stdout);
		lwc_switch(result.n_lwc, NULL, &result);
		printf("Wesh wesh wesh %p.\n", result.n_lwc);
		fflush(stdout);
	} else if (i == 0) {
		printf("Hello world from the child.\n");
		fflush(stdout);
	} else {
		printf("Error.\n");
		return -1;
	}

	return 0;
}