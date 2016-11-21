#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include "vm_tools.h"

static int __vm_pgrot_walk(	ptent_t *root,
							void *start,
							void *end,
							pgrot_walk_cb cb,
							pgrot_walk_cb alloc,
							const void *args,
							int level)
{
	int i, ret;
	int start_idx = PDX(level, start);
	int end_idx = PDX(level, end);
	void *base_va = (void*) ((unsigned long)start & ~(PDADDR(level + 1, 1)-1));

	assert(level >= 0 && level <= NPTLVLS);
	assert(end_idx < NPTENTRIES);

	for (i = start_idx; i <= end_idx; i++) {
		void *n_start_va, *n_end_va;
		void *cur_va = base_va + PDADDR(level, i); 
		ptent_t *pte = &root[i];
		
		assert(pte != NULL);

		if (alloc && !pte_present(*pte)) {
			printf("In the alloc !\n");
			cb_info info = {level, args};
			if ((ret = alloc(pte, cur_va, &info)))
				return ret;
		}
		
		/* If the page is still not present, we skip it.*/
		if (!pte_present(*pte))
			continue;

		/*If the pte is present and is a mapping.*/
		if (pte_present(*pte) && 
			(((level == 1 || level == 2) && pte_big(*pte))|| level == 0)) {
			cb_info info = {level, args};
			if ((ret = cb(pte, cur_va, &info)))
				return ret;

			continue;
		}

		n_start_va = (i == start_idx) ? start : cur_va;
		n_end_va = (i == end_idx) ? end : cur_va + PDADDR(level, 1) -1;

		ret = __vm_pgrot_walk((ptent_t*) PTE_ADDR(root[i]),
			n_start_va, n_end_va, cb, alloc, args, level - 1);
		if (ret)
			return ret;
	}

	return 0;
}

int vm_pgrot_walk(	ptent_t *root,
					void *start,
					void *end,
					pgrot_walk_cb cb,
					pgrot_walk_cb alloc,
					void *args)
{
	return __vm_pgrot_walk(root, start, end, cb, alloc, args, 3);
}


struct cow_info {
	ptent_t* o_root;
	ptent_t* n_root;
};

static int __vm_pgrot_cow(ptent_t* pte, void *va, cb_info *info)
{
	int ret;
	assert(pte_present(*pte));

	struct cow_info *inf = (struct cow_info*) (info->args);
	assert(inf != NULL);

	struct page *pg = dune_pa2page(PTE_ADDR(*pte));
	ptent_t* newRoot = inf->n_root;
	ptent_t* newPte;
	int create = CREATE_NONE;
	switch(info->level) {
		case 0:
			create = CREATE_NORMAL;
			break;
		case 1: 
			create = CREATE_BIG;
			break;
		case 2:
			create = CREATE_BIG_1GB;
			break;
		default:
			/* Error.*/
			return 1;	
	}
	
	/* Giving minimal rights for the moment.*/
	ptent_t perm = PTE_P;
	ret = vm_lookup(newRoot, va, &newPte, create , perm);

	if (dune_page_isfrompool(PTE_ADDR(*pte)))
		dune_page_get(pg);

	/* Do a cow if this is a user mapping.*/
	// if (*pte  & PTE_U) {
	// 	*pte &= ~(PTE_W);
	// 	*pte |= PTE_COW;
	// }

	/* Set the actual entry.*/
	*newPte = *pte;

	/* Fix the entries inside the intermediary entries.*/
	#define PPTE_ADDR(x) ((ptent_t*) PTE_ADDR(x))

	int i = PDX(3, va), j = PDX(2, va);
	ptent_t* o_pml4 = inf->o_root, *n_pml4 = inf->n_root;
	assert(pte_present(n_pml4[i]) && pte_present(o_pml4[i]));
	n_pml4[i] = PTE_ADDR(n_pml4[i]) | PTE_FLAGS(o_pml4[i]);

	ptent_t *o_pdpte = PPTE_ADDR(o_pml4[i]), *n_pdpte = PPTE_ADDR(n_pml4[i]);
	assert(pte_present(o_pdpte[j]) && pte_present(n_pdpte[j]));
	n_pdpte[j] = PTE_ADDR(n_pdpte[j]) | PTE_FLAGS(o_pdpte[j]);
	
	if (info->level == 2)
		assert(PTE_ADDR(n_pdpte[j]) == PTE_ADDR(o_pdpte[j]));

	if (info->level < 2) {
		int k = PDX(1, va);
		ptent_t *o_pde = PPTE_ADDR(o_pdpte[j]), *n_pde = PPTE_ADDR(n_pdpte[j]);
		assert(pte_present(o_pde[k]) && pte_present(n_pde[k]));
		n_pde[k] = PTE_ADDR(n_pde[k]) | PTE_FLAGS(o_pde[k]);
		if (info->level == 1)
			assert(PTE_ADDR(n_pde[k]) == PTE_ADDR(o_pde[k]));

		if (info->level == 0) {
			int l = PDX(0, va);
			ptent_t *o_pte = PPTE_ADDR(o_pde[k]), *n_pte = PPTE_ADDR(n_pde[k]);
			assert(pte_present(o_pte[l]) && pte_present(n_pte[l]));
			assert(PTE_ADDR(n_pte[l]) == PTE_ADDR(o_pte[l]));
			n_pte[l] = o_pte[l];
		}
	}

	return 0;
}


ptent_t* vm_pgrot_cow(ptent_t* root)
{
	int ret;
	ptent_t* newRoot;

	newRoot = memalign(PGSIZE, PGSIZE);
	if (!newRoot)
		goto err;
	memset(newRoot, 0, PGSIZE);

	//TODO: Maybe add perm from original.
	struct cow_info info = {root, newRoot};
	ret = __vm_pgrot_walk(root, VA_START, VA_END, &__vm_pgrot_cow, NULL,
		&info, 3);
	if (ret)
		goto err;

	return newRoot;
err:
	if (newRoot)
		dune_vm_free(newRoot);
	return NULL;
}

//NOTE: flags are for intermediary levels.
int vm_lookup(	ptent_t* root,
				void *va,
				ptent_t** pte_out,
				int create,
				ptent_t flags)
{
	int i, j, k, l;
	ptent_t *pml4 = root, *pdpte, *pde, *pte;
	i = PDX(3, va); j = PDX(2, va); k = PDX(1, va); l = PDX(0, va);

	if (!pte_present(pml4[i])) {
		if (!create)
			return -ENOENT;

		pdpte = alloc_page();
		memset(pdpte, 0, PGSIZE);
		pml4[i] = PTE_ADDR(pdpte) | flags;
	} 
	
	pdpte = (ptent_t*) PTE_ADDR(pml4[i]);
	
	/* Looking for a 1GB big page.*/
	if ((!pte_present(pdpte[j]) && create == CREATE_BIG_1GB) ||
		(pte_present(pdpte[j]) && pte_big(pdpte[j]))) {
		*pte_out = &pdpte[j];
	}

	if (!pte_present(pdpte[j])) {
		if (!create)
			return -ENOENT;

		pde = alloc_page();
		memset(pde, 0, PGSIZE);
		pdpte[j] = PTE_ADDR(pde) | flags;
	}

	pde = (ptent_t*) PTE_ADDR(pdpte[j]);

	/* Looking for a 2MB big page.*/
	if ((!pte_present(pde[k]) && create == CREATE_BIG) ||
		(pte_present(pde[k]) && pte_big(pde[k]))) {
		*pte_out = &pde[k];
		return 0;
	}

	if (!pte_present(pde[k])) {
		if (!create)
			return -ENOENT;

		pte = alloc_page();
		memset(pte, 0, PGSIZE);
		pde[k] = PTE_ADDR(pte) | flags;
	}

	pte = (ptent_t*) PTE_ADDR(pde[k]);
	
	*pte_out = &pte[l];
	return 0;
}