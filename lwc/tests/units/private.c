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
	void *private = mmap(address, (size_t) PGSIZE, PROT_READ | PROT_WRITE,
		MAP_ANONYMOUS, 0, 0);

	vm_addrptr start = (vm_addrptr)private;
	vm_addrptr end = start + PGSIZE;

	lwc_rsrc_spec specs;
	lwc_rg_struct to_share = {start, end, LWC_UNMAP};
	Q_INIT_HEAD(&(specs.ranges));
	Q_INIT_ELEM(&to_share, lk_rg);
	Q_INSERT_FRONT(&(specs.ranges), &(to_share), lk_rg);


	i = lwc_create(&specs, &result);
	if (i == 1) {
		printf("Parent is writing secret.\n");
		memset(private, 42, PGSIZE-1);
		printf("Wrote secret.\n");
		lwc_switch(result.n_lwc, NULL, &result);
	} else if (i  == 0) {
		printf("Child attempts to read the private data.\n");
		char *reader = private;
		//TODO: doesn't fail with proper error message but fails.
		while (reader < ((char*)private) + PGSIZE-1) {
			printf("%c", *reader);
			reader++;
		}

		printf("Child done.\n");
	}

	return 0;
}