#include "lwC.h"
#include <stdlib.h>

#define PTE_DEF_FLAGS	(PTE_P | PTE_W | PTE_U)

static inline int pte_present(ptent_t pte)
{
	return (PTE_FLAGS(pte) & PTE_P);
}

static inline int pte_RW(ptent_t pte) {

	return (PTE_FLAGS(pte) & PTE_W);
}


static inline int pte_big(ptent_t pte)
{
	return (PTE_FLAGS(pte) & PTE_PS);
}

static inline void * alloc_page(void)
{
	struct page *pg = dune_page_alloc();
	if (!pg)
		return NULL;

	return (void *) dune_page2pa(pg);
}
ptent_t* deep_copy_pgroot(ptent_t *pgroot, ptent_t *cppgroot) {

	//Create the copy for the page root.
	cppgroot = memalign(PGSIZE, PGSIZE);
	cppgroot = memcpy(cppgroot, pgroot, PGSIZE);

	//Creating copies for pml4
	ptent_t *o_pml4 = pgroot, *o_pdpte, *o_pde, *o_pte;
	ptent_t *pml4 = cppgroot, *pdpte, *pde, *pte;


	for (int i = 0; i < NB_ENTRIES_PTE; i++) {
		if (!pte_present(o_pml4[i])) {
			pml4[i] = o_pml4[i];
			continue;
		}

		//It is present so we have to create the copy.
		o_pdpte = (ptent_t*) PTE_ADDR(o_pml4[i]);
		pdpte = alloc_page();
		memset(pdpte, 0, PGSIZE);
		//TODO copy the flags.
		pml4[i] = PTE_ADDR(pdpte) | PTE_FLAGS(o_pml4[i]);

		for (int j = 0; j < NB_ENTRIES_PTE; j++) {
			//TODO Handle big too!
			if (!pte_present(o_pdpte[j])) {
				pdpte[j] = o_pdpte[i];
				continue;
			}

			if (pte_big(o_pdpte[j])) {
				pdpte[j] = o_pdpte[j];
				continue;
			}

			o_pde = (ptent_t*) PTE_ADDR(o_pdpte[j]);
			pde = alloc_page();
			memset(pde, 0 , PGSIZE);
			//TODO flags?
			pdpte[j] = PTE_ADDR(pde) | PTE_FLAGS(o_pdpte[j]);

			for (int k = 0; k < NB_ENTRIES_PTE; k++) {
				//TODO handle big too.
				if (!pte_present(o_pde[k])) {
					pde[k] = o_pde[k];
					continue;
				}

				if (pte_big(o_pde[k])) {
					pde[k] = o_pde[k];
					continue;
				}

				o_pte = (ptent_t*) PTE_ADDR(o_pde[k]);
				pte = alloc_page();
				memset(pte, 0, PGSIZE);

				pde[k] = PTE_ADDR(pte) | PTE_FLAGS(o_pde[k]);
				
				for (int l = 0; l < NB_ENTRIES_PTE; l++) {

					if (pte_present(o_pte[l])) {
						pte[l] = o_pte[l];
					} 
				}	
			}
		}

	}

	return cppgroot;
}
