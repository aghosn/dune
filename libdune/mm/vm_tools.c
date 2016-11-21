#include <errno.h>
#include "vm_tools.h"

static int __vm_pgrot_walk(	ptent_t *root,
							void *start,
							void *end,
							pgrot_walk_cb cb,
							pgrot_walk_cb alloc,
							void *args,
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

		if (alloc && !pte_present(*pte)) {
			cb_info info = {level, args};
			if ((ret = alloc(pte, cur_va, &info)))
				return ret;
		}

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

static int __vm_pgrot_cow(ptent_t* pte, void *va, cb_info *info)
{
	int ret;
	assert(pte_present(*pte));
	struct page *pg = dune_pa2page(PTE_ADDR(*pte));
	ptent_t* newRoot = (ptent_t *) (info->args);
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
	
	//TODO: check if this is correct for intermediary levels.
	ptent_t perm = PTE_FLAGS(*pte);
	ret = vm_lookup(newRoot, va, &newPte, create , perm);

	if (dune_page_isfrompool(PTE_ADDR(*pte)))
		dune_page_get(pg);

	/* Do a cow if this is a user mapping.*/
	if (*pte  & PTE_U) {
		*pte &= ~(PTE_W);
		*pte |= PTE_COW;
	}

	/* Set the actual entry.*/
	*newPte = *pte;

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
	ret = __vm_pgrot_walk(root, VA_START, VA_END, &__vm_pgrot_cow, NULL,
		newRoot, 3);
	if (ret)
		goto err;

	return newRoot;
err:
	if (newRoot)
		dune_free(newRoot);
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