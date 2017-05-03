#include <stdio.h>
#include <stdlib.h>
#include <inc/syscall.h>
#include <core/lwc_types.h>
#include <sys/mman.h>

#define SIZE (1024 * 1024)

int main(int argc, char *argv[])
{
	void* ptr = malloc(1000 *SIZE);
	if (!ptr) {
		printf("Failure.\n");
		return 1;
	}
	//printf("hello.\n");	
	memset(ptr, 0, SIZE);
	while(1) {}
	return 0;
}