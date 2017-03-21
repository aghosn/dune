#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/mman.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>

#define COUNT 1000

int main(void)
{
	int res = 0;
	int* shared_buf = (int*) mmap(NULL, 4096, PROT_READ | PROT_WRITE, 
		MAP_ANON | MAP_SHARED, -1, 0);
	if (shared_buf == MAP_FAILED) {
		perror("Mmap failed for shared.\n");
		return 0;
	}

	memset(shared_buf, 0, 4096);

	shared_buf[0] = 0;
	struct timeval tv_s = {0};
	struct timeval tv_e = {0};

	gettimeofday(&tv_s, NULL);

increment:
	res = vfork();
	if (res == 0) {
		shared_buf[0] += 1;
		exit(2);
	} else if (res != 0 && shared_buf[0] < COUNT){
		/*TODO is this needed?*/
		wait(NULL);
		goto increment;
	} else {
		gettimeofday(&tv_e, NULL);
		printf("Time in microseconds: %ld microseconds\n",
            ((tv_e.tv_sec - tv_s.tv_sec)*1000000L +tv_e.tv_usec) - tv_s.tv_usec);
	}

	return 0;
}