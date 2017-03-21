#include <time.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <inc/syscall.h>
#include <core/lwc_types.h>

#define COUNT 500

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
	//clock_t start_t, end_t;
	struct timeval tv_s = {0};
	struct timeval tv_e = {0};

	//start_t = clock();
	gettimeofday(&tv_s, NULL);

	lwc_res_t snap = snapshot(shared_buf);
	shared_buf[0] += 1;
	if (shared_buf[0] < COUNT) {
		rollback(&snap);
	}
	
	//end_t = clock();
	gettimeofday(&tv_e, NULL);
	
	//double diff_t = (double)(end_t - start_t) / CLOCKS_PER_SEC;
	//printf("Total time for %d rollback is %f seconds\n", COUNT, diff_t);
	
	printf("Time in microseconds: %ld microseconds for %d rollbacks.\n",
            ((tv_e.tv_sec - tv_s.tv_sec)*1000000L +tv_e.tv_usec) - tv_s.tv_usec, COUNT);
	return 0;
}