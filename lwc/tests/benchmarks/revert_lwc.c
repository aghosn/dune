#include <time.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <inc/syscall.h>
#include <core/lwc_types.h>

#define COUNT 1000

int main(void)
{
	lwc_res_t args;
	int res = 0;
	int* shared_buf = (int*) mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_ANON | MAP_SHARED, -1, 0);
	if(shared_buf == MAP_FAILED) {
		perror("Mmap failed for shared.\n");
		return 0;
	}

	memset(shared_buf, 0, 4096);
	shared_buf[0] = 0;

	lwc_rg_struct specs[1];
	specs[0].start = (vm_addrptr) shared_buf;
	specs[0].end = specs[0].start + 4096;
	specs[0].opt = LWC_SHARED;

	struct timeval tv_s = {0};
	struct timeval tv_e = {0};

	gettimeofday(&tv_s, NULL);

increment:
	res = lwc_create(specs, 1, &args);
	if (res == 0) {
		shared_buf[0] += 1;
		lwc_switch_discard(args.caller, NULL, &args);
	} else if (res != 0 && shared_buf[0] < COUNT) {
		lwc_switch(args.n_lwc, NULL, &args);
		goto increment;
	} else {
		gettimeofday(&tv_e, NULL);
		printf("Time in microseconds: %ld microseconds\n",
            ((tv_e.tv_sec - tv_s.tv_sec)*1000000L +tv_e.tv_usec) - tv_s.tv_usec);
	}

	return 0;
}