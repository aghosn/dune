#include <stdio.h>
//#include "lwC.h"
//#include "lwc_vm.h"

// static void pgflt_handler(uintptr_t addr, uint64_t fec, struct dune_tf *tf)
// {
// 	bool was_user = (tf->cs & 0x3);

// 	printf((was_user)? "Was in user mode\n" : "Was not in user mode\n");

// 	if (was_user) {
// 		printf("CR2 %p and fec %lu \n", addr, fec);
// 		fflush(stdout);
// 		dune_dump_trap_frame(tf);
// 		abort();
// 	}

// 	load_cr3(pgroot);
//         if (was_user) {
//                 printf("sandbox: got unexpected G3 page fault at addr %lx, fec %lx\n", addr, fec);
//                 dune_dump_trap_frame(tf);
//                 dune_ret_from_user(-EFAULT);
//         } else {
//                 ret = dune_vm_lookup(pgroot, (void *) addr, CREATE_NORMAL, &pte);
//                 assert(!ret);
//                 *pte = PTE_P | PTE_W | PTE_ADDR(dune_va_to_pa((void *) addr));
//         }
// }

int main(int argc, char *argv[]) {
	
	//dune_init(false);
	//dune_enter();
	// ptent_t* copy = lwc_copy_pgroot(pgroot);
	// ptent_t* cowcopy = lwc_cow_copy_pgroot(copy);
	// printf("I have two COW copies and one that can write.\n");

	// if (copy == NULL)
	// 	return 1;

	// do_syscall(NULL, 1);

	// dune_register_pgflt_handler(&pgflt_handler);

	
	//sandbox_init("/lib64/ld-linux-x86-64.so.2", argc, argv);


	printf("Hello world\n");
	/*
	load_cr3((unsigned long)copy);

	printf("Hello world!\n") ;*/
	return 0;
}