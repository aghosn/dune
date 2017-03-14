#include <time.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <inc/syscall.h>
#include <core/lwc_types.h>
#include <mm/memory.h>
#include <mm/mm_tools.h>

int main(int argc, char *argv[])
{	
	int i = -1;
	lwc_res_t result;
	void *address = (void*) 0x0000005eff8000;
	int *shared = mmap(address, 4096, PROT_WRITE | PROT_READ, MAP_ANON | MAP_SHARED, -1, 0);

	if (shared == MAP_FAILED) {
		perror("Unable to mmap.\n");
		return 1;
	}

	lwc_rg_struct specs[1];
	specs[0].start = (vm_addrptr) shared;
	specs[0].end = specs[0].start + 4096;
	specs[0].opt = LWC_SHARED;

	i = lwc_create(specs, 1, &result);
	
	if (i == 1) {
		printf("Hello from the parent %p\n", result.n_lwc);
		fflush(stdout);
		lwc_switch(result.n_lwc, NULL, &result);
	} else {
		printf("Hello from the child.\n");
	}
	return 0;
}