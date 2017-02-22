#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <assert.h>
#include <sandbox/sandbox.h>
#include <inc/syscall.h>
#include <core/lwc_types.h>
#include "output.h"

int main(void) {
	int i = -1;
	lwc_res_t result;


	i = lwc_create(NULL, 0, &result);
	if (i == 1) {
		
		lwc_switch_discard(result.n_lwc, NULL, &result);
		TFAILURE("Back to the parent!\n");

	} else if (i == 0) {
		TSUCCESS("In the child!\n");
	} else {
		TFAILURE("Could not create context.\n");
		return -1;
	}

	TSUCCESS("Done!\n");
	return 0;
}