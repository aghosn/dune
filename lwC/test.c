#include <stdio.h>
#include "lwC.h"
#include "lwc_vm.h"

int main(void) {
	
	dune_init(false);
	dune_enter();
	ptent_t* copy = lwc_copy_pgroot(pgroot);
	if (copy == NULL)
		return 1;
	load_cr3((unsigned long)copy);
	printf("Hello world!\n") ;
	return 0;
}