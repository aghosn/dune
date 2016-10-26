#include <stdio.h>
#include "lwC.h"
#include "lwc_vm.h"

int main(int argc, char *argv[]) {
	
	dune_init(false);
	dune_enter();
	ptent_t* copy = lwc_copy_pgroot(pgroot);
	ptent_t* cowcopy = lwc_cow_copy_pgroot(copy);
	printf("I have two COW copies and one that can write.\n");

	if (copy == NULL)
		return 1;

	do_syscall(NULL, 1);
	sandbox_init(argc, argv);
	printf("After the do syscall.\n");
	/*
	load_cr3((unsigned long)copy);

	printf("Hello world!\n") ;*/
	return 0;
}