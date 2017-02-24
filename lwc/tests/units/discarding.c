#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <assert.h>
#include <sandbox/sandbox.h>
#include <inc/syscall.h>
#include <core/lwc_types.h>
#include "output.h"

int COUNT;

int main(void)
{
	int id;
	COUNT = 4;
	lwc_res_t result;

create:
	id = lwc_create(NULL, 0, &result);
	if (id == 0 && COUNT >= 0) {
		COUNT--;
		printf("Current value %d\n", COUNT);
		goto create;
	} else if (COUNT >= 0){
		lwc_switch_discard(result.n_lwc, NULL, &result);
	}
	return 0;
}