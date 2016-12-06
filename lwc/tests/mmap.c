#include <stdio.h>
#include <stdlib.h>
#include <inc/syscall.h>
#include <core/lwc_types.h>
#include <sys/mman.h>

#define my_assert(cond) do { \
	if (cond) \
		write(1, #cond " OK\n", strlen( #cond " OK\n")); \
	else \
		write(1, #cond " FAIL\n", strlen( #cond " FAIL\n")); \
} while(0)

int main(int argc, char *argv[]) {
	int i = -1;
	lwc_res_t result;

	i = lwc_create(NULL, &result);

	if (i == 1) {
		char *p = mmap((void *) NULL, 4096, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);
		my_assert(p != MAP_FAILED);
		*p = 1;
		lwc_switch(result.n_lwc, NULL, &result);
		my_assert(2);
		my_assert(*p == 1);
	} else if (i == 0) {
		char *p = mmap((void *) NULL, 4096, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);
		my_assert(p != MAP_FAILED);
		*p = 2;
		my_assert(1);
		lwc_switch(result.caller, NULL, &result);
	} else {
		printf("Error.\n");
		return -1;
	}

	return 0;
}
