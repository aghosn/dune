#include "lwC.h"
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

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

static int lwc_page_walk(ptent_t *dir, void *start_va, void *end_va,
	lwc_page_cb cb, const void *arg, int create, int level) {

	int i, ret;
	int start_idx = PDX(level, start_va);
	int end_idx = PDX(level, end_va);

	void *base_va = (void*) ((unsigned long) 
		start_va & ~(PDADDR(level + 1, 1) - 1));
	
	assert(level >= 0 && level <= NPTLVLS);
	assert(end_idx < NPTENTRIES);

	for (i = start_idx; i <= end_idx; i++) {
		void *n_start_va, *n_end_va;
		void *cur_va = base_va + PDADDR(level, i);
		ptent_t *pte = &dir[i];

		if (level == 2) {
			if (create & LWC_CREATE_BIG_1GB || 
						(pte_present(*pte) && pte_big(*pte))) {
				if ((ret = cb(arg, pte, cur_va, level)))
					return ret;
				continue;
			}
		}

		if (level == 1) {
			if (create & LWC_CREATE_BIG || (pte_present(*pte) && pte_big(*pte))) {
				if ((ret = cb(arg, pte, cur_va, level)))
					return ret;
				continue;
			}
		}

		if (level == 0) {
			if (create & LWC_CREATE_NORMAL || pte_present(*pte)) {
				if((ret = cb(arg, pte, cur_va, level)))
					return ret;
				continue;
			}
		}

		if (!pte_present(*pte)) {
			ptent_t *new_pte;

			if (!(create & LWC_CREATE_NORMAL))
				continue;

			new_pte = alloc_page();
			if (!new_pte)
				return -ENOMEM;
			memset(new_pte, 0, PGSIZE);
			*pte = PTE_ADDR(new_pte) | PTE_DEF_FLAGS;
		}

		n_start_va = (i == start_idx)? start_va : cur_va;
		n_end_va = (i == end_idx)? end_va : cur_va + PDADDR(level, 1) -1;

		ret = lwc_page_walk((ptent_t *) PTE_ADDR(dir[i]), n_start_va, n_end_va,
							 cb, arg, create, level - 1);

		if (ret)
			return ret;
	}

	return 0;
}

//Helper to make a copy of the mappings.
//TODO increment page->ref everything.
static int __lwc_copy_pgroot_helper(const void *arg, ptent_t *_pte, void *va, 
																	int level) {
	int i, j, k, l;

	struct copy_root_t *stroot = (struct copy_root_t *) arg;
	
	ptent_t* o_pml4 = (ptent_t*) stroot->original, *o_pdpte, *o_pde, *o_pte;
	ptent_t* pml4 = (ptent_t*) stroot->copy, *pdpte, *pde, *pte; 

	i = PDX(3, va);
	j = PDX(2, va);
	k = PDX(1, va);
	l = PDX(0, va);

	//Get the proper address (everything should be present.)
	assert(pte_present(o_pml4[i]));
	o_pdpte = (ptent_t*)(PTE_ADDR(o_pml4[i]));

	assert(pte_present(o_pdpte[j])); 
	o_pde = (ptent_t*) (PTE_ADDR(o_pdpte[j]));

	assert(level < 3 && level >= 0);

	//Entry pml4 exists inside the copy or we create it.
	pdpte = (ptent_t*)(PTE_ADDR(pml4[i]));
	if (!pte_present(pml4[i])) {
		pdpte = alloc_page();
		memset(pdpte, 0, PGSIZE);
		pml4[i] = PTE_ADDR(pdpte) | PTE_FLAGS(o_pml4[i]);
	}

	//Call triggered by big pages 1GB
	if (level == 2 && pte_big(o_pdpte[j])) {
		pdpte[j] = o_pdpte[j] ;
		return 0;
	}

	//Entry pdpte exists or we create it.
	pde = (ptent_t*)(PTE_ADDR(pdpte[j]));
	if (!pte_present(pdpte[j])) {
		pde = alloc_page();
		memset(pde, 0, PGSIZE);
		pdpte[j] = PTE_ADDR(pde) | PTE_FLAGS(o_pdpte[j]);
	}

	//Call triggered by big pages 2MB
	if (level == 1 && pte_big(o_pde[k])) {
		pde[k] = o_pde[k] ;
		return 0;
	}

	assert(pte_present(o_pde[k]));
	o_pte = (ptent_t*)(PTE_ADDR(o_pde[k]));

	assert(pte_present(o_pte[l]))
	
	//Page level
	pte = (ptent_t*)(PTE_ADDR(pde[k]));
	if (!pte_present(pde[k])) {
		pte = alloc_page();
		memset(pte, 0, PGSIZE);
		pde[k] = PTE_ADDR(pte) | PTE_FLAGS(o_pde[k]);
	}

	pte[l] = o_pte[l];
	
	return 0;
}

//TODO make the page count increase.
static int __lwc_cow_helper(const void *arg, ptent_t *pte, void *va, int level) {
	ptent_t* root = (ptent_t*) arg;

	assert(pte_present(*pte));

	*pte = PTE_MAKE_COW(*pte);

	//Make the other copy COW too.
	ptent_t* o_pml4 = root, *o_pdpte, *o_pde, *o_pte;

	int i = PDX(3, va), j = PDX(2, va) , k = PDX(1, va), l = PDX(0, va);

	assert(pte_present(o_pml4[i]));
	o_pdpte = (ptent_t*) (PTE_ADDR(o_pml4[i]));
	assert(pte_present(o_pdpte[j]))

	if (level == 2) {
		o_pdpte[j] = PTE_MAKE_COW(o_pdpte[j]);
		return 0;
	}

	o_pde = (ptent_t*) (PTE_ADDR(o_pdpte[j]));
	assert(pte_present(o_pde[k]));

	if (level == 1) {
		o_pde[k] = PTE_MAKE_COW(o_pde[k]);
		return 0;
	}

	o_pte = (ptent_t*) (PTE_ADDR(o_pde[k]));
	assert(pte_present(o_pte[l]));

	o_pte[l] = PTE_MAKE_COW(o_pte[l]);
	return 0;
}

ptent_t* lwc_copy_pgroot(ptent_t* root) {
	ptent_t* copy = NULL;
	copy = alloc_page();
	if (!copy)
		return NULL;
	memset(copy, 0, PGSIZE);

	struct copy_root_t cp = {root, copy};
	if(lwc_page_walk(root, VA_START, VA_END, &__lwc_copy_pgroot_helper, 
		&cp, LWC_CREATE_NONE, 3))
		return NULL; //TODO if fail free the pages.

	return copy;
} 

//Creates a copy of pgroot and makes it COW.
ptent_t* lwc_cow_pgroot(ptent_t* pgroot) {
	ptent_t* cppgroot = alloc_page();
	memset(cppgroot, 0, PGSIZE);

	struct copy_root_t cow = {pgroot, cppgroot};
	
	if (lwc_page_walk(pgroot, VA_START, VA_END, &__lwc_copy_pgroot_helper, &cow, 
		LWC_CREATE_NONE, 3))
		return NULL; //TODO should free the page for cppgroot

	//TODO for the moment entire memory, should probably do it in another way.
	if (lwc_page_walk(cow.copy, 
		VA_START, VA_END, &__lwc_cow_helper, cow.original, LWC_CREATE_NONE, 3))
		return NULL; //TODO should free cppgroot entirely

	return cppgroot;
}


lwc_result_t lwc_create(lwc_resource_spec_t specs, uint64_t options) {
    
    //TODO copy memory
    //should trap

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


