#include <time.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <inc/syscall.h>
#include <core/lwc_types.h>

#define COUNT 20

int main(void)
{
	lwc_struct *lwcs[COUNT];
	clock_t start, end;
	void *address = (void*)0x0000005eff8000;
	int *shared_buf = (int*)mmap(address, 4096, PROT_READ | PROT_WRITE, MAP_ANON | MAP_SHARED, -1, 0);
	if (shared_buf == MAP_FAILED) {
		perror("Failed to mmap.\n");
		return 0;
	}

	lwc_rg_struct specs[1];
	specs[0].start = (vm_addrptr) address;
	specs[0].end = specs[0].start + 4096;
	specs[0].opt = LWC_SHARED;

	start = clock();
	for (int i = 0; i < COUNT; i++) {
		lwc_res_t res;
		lwc_create(specs, 1, &res);
		lwcs[i] = res.n_lwc;
	}

	end = clock();
	double diff = (double)(end - start) / CLOCKS_PER_SEC;
	printf("Total time to create %d lwcs clones is %f seconds.\n", COUNT, diff);

	return 0;
}