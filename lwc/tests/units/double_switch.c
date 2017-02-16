#include <stdio.h>
#include <stdlib.h>
#include <inc/syscall.h>
#include <core/lwc_types.h>

int main(int argc, char *argv[]) {
	int i = -1;
	lwc_res_t result;

	i = lwc_create(NULL, 0, &result);

	if (i == 1) {
		printf("1. From the parent.\n");
		fflush(stdout);

		lwc_switch(result.n_lwc, NULL, &result);
		
		printf("3. From the parent.\n");
		fflush(stdout);
	} else if (i == 0) {
		printf("2. From the child.\n");
		fflush(stdout);
		
		lwc_switch(result.caller, NULL, &result);

		assert(0);
	} else {
		printf("Error.\n");
		return -1;
	}

	return 0;
}