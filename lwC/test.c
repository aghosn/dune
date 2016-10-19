#include <stdio.h>
#include "lwC.h"

int main(void) {
	
	dune_init(false);
	dune_enter();
	
	ptent_t* copy = NULL;
	copy = deep_copy_pgroot(pgroot, copy);
	printf("Hello world! %p\n", copy) ;
	return 0;
}