#include <stdio.h>
#include <stdio.h>
#include <time.h>
#include <sys/mman.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>

#define COUNT 100

int main(void) {

	int* shared_buf = (int*) mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_ANON | MAP_SHARED, -1, 0);
	if(shared_buf == MAP_FAILED) {
		perror("Mmap failed for shared.\n");
		return 0;
	}

	memset(shared_buf, 0, 4096);

	shared_buf[0] = 0;
	time_t start_t, end_t;

	time(&start_t);

increment:
	printf("Incrementing.\n");
	shared_buf[0] += 1;

	int res = fork();
	if (res == 0) {
		if (shared_buf[0] < COUNT) {
			goto increment;
		} else {
			time(&end_t);
			double diff_t = difftime(end_t, start_t);
			printf("Total time for %d rollback is %f seconds\n", COUNT, diff_t);
		}
	} else {
		wait(NULL);
	}

	return 0;
}