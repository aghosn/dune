#include <stdio.h>
#include <inc/syscall.h>
#include <core/lwc_types.h>

int main(int argc, char *argv[]) {

	lwc_struct *test = NULL;
	lwc_rsrc_spec *mod = NULL;
	test = lwc_create(mod);
	printf("Hello world\n");
	
	if (test) {
		printf("Calling switch.\n");
		fflush(stdout);
		lwc_switch(test, NULL);
		printf("After having called switch\n");
		fflush(stdout);
	}
	else
		printf("I'm the child so ...\n");
	// printf("Hello world!\n");
	// int res = lwc_fake_create();
	// if (res == 0){
	// 	lwc_fake_switch();
	// 	printf("After the switch.\n");
	// } else {
	// 	printf("I'm a child.\n");
	// }
	return 0;
}