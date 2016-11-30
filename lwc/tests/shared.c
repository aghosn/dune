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
	//TODO do an mmap here.
	void *shared = mmap(NULL, (size_t)(1 << 12), PROT_READ | PROT_WRITE, MAP_ANONYMOUS, 0, 0);
	
	printf("Shared is %p\n", shared);
	fflush(stdout);
	
	vm_addrptr start = (vm_addrptr)shared;
	vm_addrptr end = start + PGSIZE;

	lwc_rsrc_spec specs;
	lwc_rg_struct to_share = {start, end, LWC_SHARED};
	Q_INIT_HEAD(&(specs.ranges));
	Q_INIT_ELEM(&to_share, lk_rg);
	Q_INSERT_FRONT(&(specs.ranges), &(to_share), lk_rg);

	i = lwc_create(&specs, &result);

	if (i == 1) {
		printf("Hello world from the parent.\n");
		printf("The result %p and new lwc %p\n", &result, result.n_lwc);
		fflush(stdout);
		lwc_switch(result.n_lwc, NULL, &result);
		printf("Wesh wesh wesh %p.\n", result.n_lwc);
		fflush(stdout);
	} else if (i == 0) {
		printf("Hello world from the child.\n");
		assert(0);
		fflush(stdout);
	} else {
		printf("Error.\n");
		return -1;
	}

	return 0;
}