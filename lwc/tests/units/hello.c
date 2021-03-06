#include <stdio.h>
#include <stdlib.h>
#include <inc/syscall.h>
#include <core/lwc_types.h>
#include <sys/mman.h>

int main(int argc, char *argv[]) {
	int i = -1;
	lwc_res_t result;
	i = lwc_create(NULL, 0, &result);
	if (i == 1) {
		printf("Hello world from the parent.\n");
		lwc_switch(result.n_lwc, NULL, &result);
	} else if (i == 0) {
		printf("Hello world from the child.\n");
	} else {
		printf("Error bad result from call.\n");
		return -1;
	}

	return 0;
}