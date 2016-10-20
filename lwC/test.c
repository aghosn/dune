#include <stdio.h>
#include "lwC.h"

int main(void) {
	
	dune_init(false);
	dune_enter();
	ptent_t* copy = NULL;
	copy = lwc_cow_pgroot(pgroot, copy);
	printf("Hello world!\n") ;
	return 0;
}