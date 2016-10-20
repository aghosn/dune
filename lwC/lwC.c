#include "lwC.h"
#include <stdlib.h>

/*      Static helper function      */

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

//TODO optimize and/or rewrite with page_walk
ptent_t* lwc_cow_pgroot(ptent_t* pgroot, ptent_t *cppgroot) {
	cppgroot = alloc_page();

	ptent_t *o_pml4 = pgroot, *o_pdpte, *o_pde, *o_pte;
	ptent_t *pml4 = cppgroot, *pdpte, *pde, *pte;

	for (int i = 0; i < NB_ENTRIES_PTE; i++) {
		if (!pte_present(o_pml4[i])) {
			pml4[i] = o_pml4[i];
			continue;
		}

		o_pdpte = (ptent_t*) PTE_ADDR(o_pml4[i]);
		pdpte = alloc_page();
		pml4[i] = PTE_ADDR(pdpte) | PTE_FLAGS(o_pml4[i]);

		for (int j = 0; j < NB_ENTRIES_PTE; j++) {
			if (!pte_present(o_pdpte[j])) {
				pdpte[j] = o_pdpte[i];
				continue;
			}

			if (pte_big(o_pdpte[j])) {
				pdpte[j] = o_pdpte[j] = PTE_MAKE_COW(o_pdpte[j]);
				continue;
			}

			o_pde = (ptent_t*) PTE_ADDR(o_pdpte[j]);
			pde = alloc_page();
			pdpte[j] = PTE_ADDR(pde) | PTE_FLAGS(o_pdpte[j]);

			for (int k = 0; k < NB_ENTRIES_PTE; k++) {
				if (!pte_present(o_pde[k])) {
					pde[k] = o_pde[k];
					continue;
				}

				if (pte_big(o_pde[k])) {
					pde[k] = o_pde[k] = PTE_MAKE_COW(o_pde[k]);
					continue;
				}

				o_pte = (ptent_t*) PTE_ADDR(o_pde[k]);
				pte = alloc_page();
				pde[k] = PTE_ADDR(pte) | PTE_FLAGS(o_pde[k]);
				
				for (int l = 0; l < NB_ENTRIES_PTE; l++) {
					if (!pte_present(o_pte[l])) {
						pte[l] = o_pte[l];
						continue;
					}

					pte[l] = o_pte[l] = PTE_MAKE_COW(pte[l]);
				}	
			}
		}
	}

	return cppgroot;
}

lwc_result_t lwc_create(lwc_resource_spec_t specs, uint64_t options) {
    
    //TODO copy memory
    
    //TODO copy credentials
    //TODO copy syscall
    //TODO get filedescriptor.
    
    lwc_result_t t;
    return t;
}

lwc_result_t lwc_switch(lwc_context_t l, void* args) {
	//TODO implement. Need to load vm, and registers.
	
	lwc_result_t t;
	return t;
}


