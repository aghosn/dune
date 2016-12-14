#include <stdio.h>
#include <stdlib.h>
#include <inc/syscall.h>
#include <core/lwc_types.h>
#include <sys/mman.h>

int main(int argc, char *argv[]) {
	int i = -1;
	lwc_res_t result;

	i = lwc_create(NULL, &result);

	if (i == 1) {
		//TODO: set the registers.
		//list of registers: rbx rcx rdx rsi rdi r8 r9 r10 r11 r12 r13 r14 r15
		asm("mov $1, %%rbx;" : : : "rbx");
		asm("mov $2, %%rcx;" : : : "rcx");
		asm("mov $3, %%rdx;" : : : "rdx");
		asm("mov $4, %%rdi;" : : : "rdi");
		asm("mov $5, %%rsi;" : : : "rsi");
		asm("mov $8, %%r8;" : : : "r8");
		asm("mov $9, %%r9;" : : : "r9");
		asm("mov $10, %%r10;" : : : "r10");
		asm("mov $11, %%r11;" : : : "r11");
		asm("mov $12, %%r12;" : : : "r12");
		asm("mov $13, %%r13;" : : : "r13");
		asm("mov $14, %%r14;" : : : "r14");
		asm("mov $15, %%r15;" : : : "r15");
		
		lwc_switch(result.n_lwc, NULL, &result);
		uint64_t value;
		//FIXME: gets changed to 21.
		asm("mov %%rbx, %0" : "=r" (value));
		lwc_println((void*)value, D_NORMA, 1);
		
		//stays equal to 2.
		asm("mov %%rcx, %0" : "=r" (value));
		lwc_println((void*)value, D_NORMA, 2);
		
		//FIXME: gets changed to 1.
		asm("mov %%rdx, %0" : "=r" (value));
		lwc_println((void*)value, D_NORMA, 3);
		
		//FIXME: gets changed another value.
		asm("mov %%rdi, %0" : "=r" (value));
		lwc_println((void*)value, D_NORMA, 4);
		
		//FIXME: gets changed to 1.
		asm("mov %%rsi, %0" : "=r" (value));
		lwc_println((void*)value, D_NORMA, 5);
		
		//FIXME: gets changed to 0.
		asm("mov %%r8, %0" : "=r" (value));
		lwc_println((void*)value, D_NORMA, 8);
		
		//FIXME: gets changed to 0.
		asm("mov %%r9, %0" : "=r" (value));
		lwc_println((void*)value, D_NORMA, 9);
		
		//FIXME: gets changed to 0.
		asm("mov %%r10, %0" : "=r" (value));
		lwc_println((void*)value, D_NORMA, 10);
		
		//FIXME: gets changed but probably previous call.
		asm("mov %%r11, %0" : "=r" (value));
		lwc_println((void*)value, D_NORMA, 11);
		
		//FIXME: gets changed to 32
		asm("mov %%r12, %0" : "=r" (value));
		lwc_println((void*)value, D_NORMA, 12);
		
		//FIXME: gets changed to 33
		asm("mov %%r13, %0" : "=r" (value));
		lwc_println((void*)value, D_NORMA, 13);
		
		//FIXME: gets changed to 34
		asm("mov %%r14, %0" : "=r" (value));
		lwc_println((void*)value, D_NORMA, 14);
		
		//FIXME: gets changed to 35
		asm("mov %%r15, %0" : "=r" (value));
		lwc_println((void*)value, D_NORMA, 15);
	} else if (i == 0) {
		//TODO: change the registers.
		asm("mov $21, %%rbx;" : : : "rbx");
		asm("mov $22, %%rcx;" : : : "rcx");
		asm("mov $23, %%rdx;" : : : "rdx");
		asm("mov $24, %%rdi;" : : : "rdi");
		asm("mov $25, %%rsi;" : : : "rsi");
		asm("mov $28, %%r8;" : : : "r8");
		asm("mov $29, %%r9;" : : : "r9");
		asm("mov $30, %%r10;" : : : "r10");
		asm("mov $31, %%r11;" : : : "r11");
		asm("mov $32, %%r12;" : : : "r12");
		asm("mov $33, %%r13;" : : : "r13");
		asm("mov $34, %%r14;" : : : "r14");
		asm("mov $35, %%r15;" : : : "r15");
		lwc_switch(result.caller, NULL, &result);

	} else {
		printf("Error.\n");
		return -1;
	}

	return 0;
}
