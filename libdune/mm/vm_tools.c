#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include "vm_tools.h"
#include "vma.h"

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

static int __vm_pgrot_copy_v2(ptent_t *pte, void *va, cb_info *info)
{
	int ret;
	struct copy_info *inf = (struct copy_info*) (info->args);
	assert(inf != NULL);
	ptent_t *newRoot = inf->n_root;

	int create = CREATE_NONE;
	switch (info->level) {
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

	/* Give all permissions by default.
	 * Only the final entry matters.*/
	ptent_t *pte_out = NULL;
	ptent_t perm = PTE_P | PTE_U | PTE_W;
	ret = vm_lookup(newRoot, va, &pte_out, create, perm);
	ASSERT_DBG(ret == 0, "Problem{%d} while looking up address.\n", ret);

	if (inf->type != CB_COW || (PPTE_FLAGS(*pte) & PTE_COW)
		|| !(PPTE_FLAGS(*pte) & PTE_U)) {
		goto set_entry;
	}

	/* We COW the mapping.*/
	*pte &=~(PTE_W);
	*pte |= PTE_COW;
	dune_flush_tlb_one((unsigned long)va);

set_entry:
	if ((inf->type == CB_RO || inf->type == CB_SHARE)
		&& (PPTE_FLAGS(*pte) & PTE_COW)) {

		if (dune_page_isfrompool(PTE_ADDR(*pte))) {
			struct page *pg = dune_pa2page(PTE_ADDR(*pte));
			/* Need to replace the page.*/
			if (pg->ref > 1) {
				/*TODO not handling big pages yet.*/
				ASSERT_DBG(!pte_big(*pte), "Oups, a big page is cowed.\n");
				void *newPage = alloc_page();
				assert(newPage);
				memcpy(newPage, (void*)PGADDR(va), PGSIZE);

				/* Give back the old page.*/
				dune_page_put(pg);

				*pte = PTE_ADDR(newPage) | PPTE_FLAGS(*pte);
				
				/* Quickly uncow.*/
				*pte &=~(PTE_COW);
				*pte |= (PTE_W);

				dune_flush_tlb_one((unsigned long)va);
			}
		}
	}

	*pte_out = *pte;

	/* Add another reference to the page.*/
	if (dune_page_isfrompool(PTE_ADDR(*pte)))
		dune_page_get(dune_pa2page(PTE_ADDR(*pte)));

	if (inf->type == CB_RO)
		*pte_out &=~(PTE_W);

	return 0;
}

static ptent_t* vm_pgrot_fast_copy(ptent_t* root, bool cow)
{
	int ret;
	ptent_t* new_root;

	new_root = (ptent_t*) dune_page2pa(dune_page_alloc());
	if (!new_root)
		goto err;
	memset(new_root, 0, PGSIZE);

	/* pml4 level copy optimization*/
	for (int i = 0; i < NPTENTRIES; i++) {
		if (!pte_present(root[i]))
			continue;
		
		/* Kernel entry can be transposed as is.*/
		if (!(root[i] & PTE_U) || !(root[i] & PTE_W)) {
			new_root[i] = root[i];
			continue;
		}

		struct copy_info info = {root, new_root, true, CB_COW};
		ret = __vm_pgrot_walk(root,(void*)RPDX(i, 0, 0, 0),(void*)RPDX(i+1, 0, 0, 0) - 1, 
			&__vm_pgrot_copy_v2, NULL, &info, 3);
		
		if (ret)
			goto err;

	}
	
	return new_root;
err:
	if (new_root)
		dune_vm_free(new_root);
	return NULL;
}

ptent_t* vm_pgrot_copy(ptent_t* root, bool cow)
{
	int ret;
	ptent_t* newRoot;

	newRoot = (ptent_t*) dune_page2pa(dune_page_alloc());
	if (!newRoot)
		goto err;
	memset(newRoot, 0, PGSIZE);

	/* __vm_pgrot_copy copies the flags for intermediary levels for us.*/
	struct copy_info info = {root, newRoot, cow, CB_COW};
	ret = __vm_pgrot_walk(root, VA_START, VA_END, &__vm_pgrot_copy_v2, NULL,
		&info, 3);
	if (ret)
		goto err;

	return newRoot;
err:
	if (newRoot)
		dune_vm_free(newRoot);
	return NULL;
}

ptent_t* vm_pgrot_copy_range(	ptent_t *original,
								ptent_t *copy,
								void *start,
								void* end,
								bool cow,
								copy_type modifier)
{
	int ret;
	if (!copy || !original)
		goto err;

	struct copy_info info = {original, copy, cow, modifier};

	ret = __vm_pgrot_walk(original, (void*)MM_PGALIGN_DN((vm_addrptr)start),
			(void*)MM_PGALIGN_UP((vm_addrptr) end) - 1,
			&__vm_pgrot_copy_v2, NULL, &info, 3);
	if (ret)
		goto err;
	
	return copy;
err:
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

int vm_uncow(ptent_t* root, void *addr)
{
	int rc;
	ptent_t *pte = NULL;
	assert(root);
	/* Get the faulty entry.*/
	rc = vm_lookup(pgroot,(void*)addr, &pte, CREATE_NONE, 0);

	/* Check that the entry is a COW.*/
	assert(rc == 0);
	assert(PTE_U & *pte);
	assert(!(*pte & PTE_W));
	assert(*pte & PTE_COW);

	//TODO: will have to handle that if it is not correct.
	assert(!pte_big(*pte));
	
	/* If the entry is a cow, we fix it.*/
	void *newPage;
	struct page *pg = dune_pa2page(PTE_ADDR(*pte));
	ptent_t perm = PPTE_FLAGS(*pte);

	perm &= ~(PTE_COW);
	perm |= PTE_W;

	/* Only one reference to this page, so we simply keep it.*/
	if (dune_page_isfrompool(PTE_ADDR(*pte)) && pg->ref == 1) {
		*pte = PTE_ADDR(*pte) | perm;
		return 0;
	}
	
	/* We duplicate the page.*/
	newPage = alloc_page();
	assert(newPage);
	memcpy(newPage, (void*)PGADDR(addr), PGSIZE);

	/* map the page.*/
	if (dune_page_isfrompool(PTE_ADDR(*pte))) {
		dune_page_put(pg);
	}
	*pte = PTE_ADDR(newPage) | perm;

	/* Invalidate address in tlb.*/
	dune_flush_tlb_one((unsigned long)addr);
	asm("mov %cr3,%rax; mov %rax,%cr3");
	return 0;
}

static int __vm_compare_pgroots(ptent_t* pte, void *va, cb_info *args)
{
	int ret;
	ptent_t *out = NULL;
	ptent_t* other = (ptent_t*)(args->args);
	ret = dune_vm_has_mapping(other, va);
	assert(ret == 0);

	ret = vm_lookup(other, va, &out, CREATE_NONE, 0);
	assert(ret == 0);

	assert(PPTE_FLAGS(*out) == PPTE_FLAGS(*pte));
	return 0;
}

int vm_compare_pgroots(ptent_t* o, ptent_t *c)
{
	int r;
	
	r = vm_pgrot_walk(o, VA_START, VA_END, &__vm_compare_pgroots, NULL, c);
	assert(r == 0);

	r = vm_pgrot_walk(c, VA_START, VA_END, &__vm_compare_pgroots, NULL, o);
	assert(r == 0);
	return 0;
}

int vm_check_references(ptent_t* pml4, uint64_t expect_pte, uint64_t expect_o, uint64_t read_only)
{
	int i, j, k, l;
	ptent_t* pdpte, *pde, *pte;

	for (i = 0; i < NPTENTRIES; i++) {
		if (!pte_present(pml4[i]))
			continue;

		pdpte = (ptent_t*) PTE_ADDR(pml4[i]);
		for (j = 0; j < NPTENTRIES; j++) {
			if (!pte_present(pdpte[j]) || pte_big(pdpte[j]))
				continue;

			pde = (ptent_t*) PTE_ADDR(pdpte[j]);
			for (k = 0; k < NPTENTRIES; k++) {
				if (!pte_present(pde[k]) || pte_big(pde[k]))
					continue;

				pte = (ptent_t*) PTE_ADDR(pde[k]);
				for (l = 0; l < NPTENTRIES; l++) {
					if (!pte_present(pte[l]))
						continue;

					struct page *pg = dune_pa2page(PTE_ADDR(pte[l]));

					if (dune_page_isfrompool(PTE_ADDR(pte[l]))) {
						if (!(pte[l] & PTE_W))
							ASSERT_DBG(pg->ref == read_only,"PTE bad ref count %lu, user: %d, %d %d %d %d\n", pg->ref, (pte[l] & PTE_W) != 0, i, j, k, l);
						else
							ASSERT_DBG(pg->ref == expect_pte, "PTE bad ref count %lu, user: %d\n", pg->ref, (pte[l] & PTE_W) != 0);
					}
				}

				struct page *pg = dune_pa2page(PTE_ADDR(pde[k]));

				if (dune_page_isfrompool(PTE_ADDR(pde[k]))) {
					ASSERT_DBG(pg->ref == expect_o, "PDE bad ref count %lu\n", pg->ref);
				}
			}

			struct page *pg = dune_pa2page(PTE_ADDR(pdpte[j]));

			if (dune_page_isfrompool(PTE_ADDR(pdpte[j]))) {
				ASSERT_DBG(pg->ref == expect_o, "PDPTE bad ref count %lu\n", pg->ref);
			}
		}

		struct page *pg = dune_pa2page(PTE_ADDR(pml4[i]));

		if (dune_page_isfrompool(PTE_ADDR(pml4[i]))) {
			ASSERT_DBG(pg->ref == expect_o, "PML4 bad ref count %lu\n", pg->ref);
		}
	}

	return 0;
}

