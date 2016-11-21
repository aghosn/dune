#include <stdio.h>
#include <stdlib.h>
#include <dune.h>
#include <mm/vm_tools.h>

int test_swap()
{	
	printf("Swapping the pageroot.\n");
	ptent_t* newRoot = vm_pgrot_cow(pgroot);
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