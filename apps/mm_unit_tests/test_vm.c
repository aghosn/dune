#include <stdio.h>
#include <stdlib.h>
#include <dune.h>
#include <mm/vm_tools.h>

static int __compare_pgroots(ptent_t* pte, void *va, cb_info *args)
{
	int ret;
	ptent_t *out;
	ptent_t* other = (ptent_t*)(args->args);
	ret = dune_vm_has_mapping(other, va);
	if (ret) {
		printf("First one has a mapping for 0x%p, that is missing.\n", va);
		return ret;
	}

	ret = vm_lookup(other, va, &out, CREATE_NONE, 0);
	if (ret) {
		printf("VM lookup didn't work for va 0x%p.\n", va);
		return ret;
	}

	if (PPTE_FLAGS(*out) != PPTE_FLAGS(*pte)) {
		printf("Flags are different.");
		printf("0x%016lx-0x%016lx\n", PTE_FLAGS(*out), PTE_FLAGS(*pte));
		return 1;
	}

	return 0;
}

int compare_pgroots(ptent_t* o, ptent_t* c)
{
	int r = vm_pgrot_walk(o, VA_START, VA_END, &__compare_pgroots, NULL, c);
	if (r) {
		printf("Failed in test 1.\n");
		return r;
	}

	r = vm_pgrot_walk(c, VA_START, VA_END, &__compare_pgroots, NULL, o);
	if (r) {
		printf("Failed in 2.\n");
		return r;
	}

	return 0;
}


int test_swap()
{	
	printf("Swapping the pageroot.\n");
	ptent_t* newRoot = vm_pgrot_cow(pgroot);
	printf("Comparing the page root.\n");
	compare_pgroots(pgroot, newRoot);
	printf("After the comparision.\n");
	fflush(stdout);
	if (!newRoot) {
		printf("Oh Oh\n");
		return -1;
	}
	load_cr3((unsigned long) newRoot);
	printf("After the switch and everything is fine.\n");
	load_cr3((unsigned long)pgroot);
	printf("Reverted to previous page root, everything was fine.\n");
	return 0;
}


int main(void)
{
	int ret;
	printf("Initializing dune.\n");
	if ((ret = dune_init(false)))
		return -1;

	if ((ret = dune_enter()))
		return -1;

	printf("Dune initialized.\n");
	//TODO: do dune init and all of that.
	printf("Starting vm test.\n");
	if ((ret = test_swap()))
		return -1;
	return 0;
}