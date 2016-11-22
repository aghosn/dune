#include <stdio.h>
#include <inc/syscall.h>
#include <core/lwc_types.h>

int main(int argc, char *argv[]) {

	// printf("Hello world!\n");
	lwc_struct *test = NULL;
	lwc_rsrc_spec *mod = NULL;
	// printf("Before the create.\n");
	test = lwc_create(mod);
	printf("Hello world\n");
	// fflush(stdout);
	if (test) {
		printf("Calling switch.\n");
		fflush(stdout);
		lwc_switch(test, NULL);
		printf("After having called switch\n");
		fflush(stdout);
	} else
		printf("I'm the child so ...\n");
	return 0;
}