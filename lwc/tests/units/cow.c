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
	void *cowed = mmap(address, (size_t) PGSIZE, PROT_READ | PROT_WRITE,
		MAP_ANONYMOUS, 0, 0);

	memset(cowed, 43, PGSIZE-1);

	i = lwc_create(NULL, 0, &result);
	if (i == 1) {
		
		memset(cowed, 42, PGSIZE-1);
		
		lwc_switch(result.n_lwc, NULL, &result);
	} else if (i  == 0) {
		
		char *reader = cowed;
		//TODO: doesn't fail with proper error message but fails.
		while (reader < ((char*)cowed) + PGSIZE-1) {
			if (*reader != 43) {
				TFAILURE("Error: value not correct\n");
				return -1;
			}

			reader++;
		}

		TSUCCESS("Done!\n");
	}

	return 0;
}