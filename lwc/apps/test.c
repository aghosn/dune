#include <stdio.h>
#include <inc/syscall.h>
#include <core/lwc_types.h>

int main(int argc, char *argv[]) {
	// lwc_result_t* sp = lwc_create(NULL);
	// if (!sp) goto err;

	// printf("Hello world");
	// fflush(stdout);

	// if (sp->n_lwc) {
	// 	printf(" from the parent.\n");
	// 	fflush(stdout);
	// 	lwc_switch(sp->n_lwc, NULL);
	// } else {
	// 	printf(" from the child.\n");
	// 	fflush(stdout);
	// }
	lwc_struct *safe_point = NULL;
	lwc_rsrc_spec *mod = NULL;	
	safe_point = lwc_create(mod);
	printf("Hello world");
	fflush(stdout);
	if (safe_point) {
		printf(" from the parent.\n");
		fflush(stdout);
		lwc_switch(safe_point, NULL);
	} else {
		printf(" from the child.\n");
	}

	return 0;
// err:
// 	printf("Error!\n");
// 	fflush(stdout);
//	return 1;
}