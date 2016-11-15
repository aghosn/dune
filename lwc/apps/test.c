#include <stdio.h>
#include <inc/syscall.h>
#include <core/lwc_types.h>

int main(int argc, char *argv[]) {

	printf("Hello world\n");
	lwc_struct *test = NULL;
	lwc_rsrc_spec *mod = NULL;
	test = lwc_create(mod);
	lwc_switch(NULL, NULL);
	return 0;
}