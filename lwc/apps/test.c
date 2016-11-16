#include <stdio.h>
#include <inc/syscall.h>
#include <core/lwc_types.h>

int main(int argc, char *argv[]) {

	lwc_struct *test = NULL;
	lwc_rsrc_spec *mod = NULL;
	test = lwc_create(mod);
	printf("Hello world %p\n", &test);
	fflush(stdout);
	if (test)
		lwc_switch(test, NULL);
	else
		printf("I'm the child so ...\n");
	return 0;
}