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
	void *cowed = mmap(address, (size_t) PGSIZE, PROT_READ | PROT_WRITE,
		MAP_ANONYMOUS, 0, 0);

	memset(cowed, 43, PGSIZE-1);

	i = lwc_create(NULL, &result);
	if (i == 1) {
		printf("Parent is writing his value.\n");
		memset(cowed, 42, PGSIZE-1);
		printf("Wrote value.\n");
		lwc_switch(result.n_lwc, NULL, &result);
	} else if (i  == 0) {
		printf("Child attempts to read the data.\n");
		char *reader = cowed;
		//TODO: doesn't fail with proper error message but fails.
		while (reader < cowed + PGSIZE-1) {
			if (*reader != 43) {
				printf("Error, value not correct %c\n", *reader);
				return -1;
			}

			reader++;
		}

		printf("Done with success.\n");
	}

	return 0;
}