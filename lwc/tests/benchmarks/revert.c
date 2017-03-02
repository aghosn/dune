#include <time.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <inc/syscall.h>
#include <core/lwc_types.h>

#define COUNT 100

lwc_res_t snapshot(int* shared)
{
	lwc_res_t res;
	int i = 0;

	lwc_rg_struct specs[1];
	specs[0].start = (vm_addrptr) shared;
	specs[0].end = specs[0].start + 4096;
	specs[0].opt = LWC_SHARED;

	i = lwc_create(specs, 1, &res);

	/* Parent*/
	if (i == 1) {
		return res;

	/* Child, we need to reprovision the snapshot.*/
	} else if (i == 0) {
		return snapshot(shared);
	} else {
		perror("Something went wrong during the snapshot.\n");
		abort();
	}

	perror("Should not have reached this part.\n");
	abort();
	return res;
}

void rollback(lwc_res_t *res)
{
	lwc_res_t tar;
	lwc_switch_discard(res->n_lwc, NULL, &tar);
	perror("Rollback returned!\n");
	abort();
}

int main(void)
{
	printf("Starting benchmark revert for %d.\n", COUNT);
	void *address = (void*)0x0000005eff8000;
	int *shared_buf = (int*)mmap(address, 4096, PROT_READ | PROT_WRITE, MAP_ANON | MAP_SHARED, -1, 0);
	if (shared_buf == MAP_FAILED) {
		perror("Mmap failed for shared.\n");
		return 0;
	}

	memset(shared_buf, 0, 4096);

	shared_buf[0] = 0;
	time_t start_t, end_t;

	time(&start_t);

	lwc_res_t snap = snapshot(shared_buf);
	shared_buf[0] += 1;

	if (shared_buf[0] < COUNT) {
		rollback(&snap);
	}
	
	time(&end_t);
	
	double diff_t = difftime(end_t, start_t);
	
	printf("Total time for %d rollback is %f seconds\n", COUNT, diff_t);
	return 0;
}