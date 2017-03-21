#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <inc/syscall.h>
#include <core/lwc_types.h>
#include <sys/mman.h>

int main(int argc, char *argv[]) {
	int i = -1;
	lwc_res_t result;
	char *p = NULL;

	i = lwc_create(NULL, 0, &result);

	if (i == 1) {
		p = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_PRIVATE
			| MAP_ANONYMOUS, 0, 0);
		assert(p != MAP_FAILED);
		*p = 1;
		lwc_switch(result.n_lwc, NULL, &result);
		assert(2);
		assert(*p == 1);
	} else if (i == 0) {
		p = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_PRIVATE
			| MAP_ANONYMOUS, 0, 0);
		assert(p != MAP_FAILED);
		*p = 2;
		assert(1);
		assert(*p == 2);
		lwc_switch(result.caller, NULL, &result);
	} else {
		printf("Error.\n");
		return -1;
	}

	return 0;
}
