#include <stdio.h>
#include <stdlib.h>
#include <dune.h>
#include <mm/vm_tools.h>

int compare_pgroots(ptent_t* o, ptent_t* c)
{
	int i, j, k, l;
	ptent_t* pml4 = o, *pdpte, *pde, *pte;
	ptent_t* c_pml4 = c, *c_pdpte, *c_pde, *c_pte;
	for (i = 0; i < NPTENTRIES; i++) {
		assert(PTE_FLAGS(pml4[i]) == PTE_FLAGS(c_pml4[i]));

		if (!pte_present(pml4[i]))
			continue;
		pdpte = (ptent_t*) PTE_ADDR(pml4[i]);
		c_pdpte = (ptent_t*) PTE_ADDR(c_pml4[i]);
		for (j = 0; j < NPTENTRIES; j++) {
			if (PTE_FLAGS(pdpte[j]) != PTE_FLAGS(c_pdpte[j]))
				printf("%d: %lx - %lx\n", j, PTE_FLAGS(pdpte[j]), PTE_FLAGS(c_pdpte[j]));
			assert(PTE_FLAGS(pdpte[j]) == PTE_FLAGS(c_pdpte[j]));

			if (!pte_present(pdpte[j]))
				continue;

			pde = (ptent_t*) PTE_ADDR(pdpte[j]);
			c_pde = (ptent_t*) PTE_ADDR(c_pdpte[j]);
			for (k = 0; k < NPTENTRIES; k++) {
				assert(PTE_FLAGS(pde[k]) == PTE_FLAGS(c_pde[k]));

				if (!pte_present(pde[k]))
					continue;

				pte = (ptent_t*) PTE_ADDR(pde[k]);
				c_pte = (ptent_t*) PTE_ADDR(c_pde[k]);
				for (l = 0; l < NPTENTRIES; l++) {
					//TODO: debug this generates a EPT violation.
					//assert(pte[l] == c_pte[l]);
				}
			}
		}
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