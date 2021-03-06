#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

#include "../dune.h"
#include "vm.h"
#include "vm_tools.h"

ptent_t get_pte_perm(int perm)
{
	ptent_t pte_perm = 0;

	//TODO: urgent!! shared support.
	if (perm & PERM_R)
		pte_perm = PTE_P;
	if (perm & PERM_W)
		pte_perm |= PTE_W;
	if (!(perm & PERM_X))
		pte_perm |= PTE_NX;
	if (perm & PERM_U)
		pte_perm |= PTE_U;
	if (perm & PERM_UC)
		pte_perm |= PTE_PCD;
	if (perm & PERM_COW)
		pte_perm |= PTE_COW;
	if (perm & PERM_USR1)
		pte_perm |= PTE_USR1;
	if (perm & PERM_USR2)
		pte_perm |= PTE_USR2;
	if (perm & PERM_USR3)
		pte_perm |= PTE_USR3;
	if (perm & PERM_BIG || perm & PERM_BIG_1GB)
		pte_perm |= PTE_PS;

	return pte_perm;
}

void * alloc_page(void)
{	
	struct page *pg = dune_page_alloc();
	if (!pg)
		return NULL;

	return (void *) dune_page2pa(pg);
}

void put_page(void * page)
{
	// XXX: Using PA == VA
	
	struct page *pg = dune_pa2page((physaddr_t)page);
	if (pg < pages || pg >=(pages + num_pages))
		return;

	dune_page_put(pg);
}

static int __dune_vm_page_walk(ptent_t *dir, void *start_va, void *end_va,
			page_walk_cb cb, const void *arg, int level,
			int create)
{
	// XXX: Using PA == VA
	int i, ret;
	int start_idx = PDX(level, start_va);
	int end_idx = PDX(level, end_va);
	void *base_va = (void *) ((unsigned long)
			start_va & ~(PDADDR(level + 1, 1) - 1));

	assert(level >= 0 && level <= NPTLVLS);
	assert(end_idx < NPTENTRIES);

	for (i = start_idx; i <= end_idx; i++) {
		void *n_start_va, *n_end_va;
		void *cur_va = base_va + PDADDR(level, i);
		ptent_t *pte = &dir[i];

		if (level == 0) {
			if (create == CREATE_NORMAL || *pte) {
				ret = cb(arg, pte, cur_va);
				if (ret)
					return ret;
			}
			continue;
		}

		if (level == 1) {
			if (create == CREATE_BIG || pte_big(*pte)) {
				ret = cb(arg, pte, cur_va);
				if (ret)
					return ret;
				continue;
			}
		}

		if (level == 2) {
			if (create == CREATE_BIG_1GB || pte_big(*pte)) {
				ret = cb(arg, pte, cur_va);
				if (ret)
					return ret;
				continue;
			}
		}

		if (!pte_present(*pte)) {
			ptent_t *new_pte;

			if (!create)
				continue;
			
			new_pte = alloc_page();
			if (!new_pte)
				return -ENOMEM;
			memset(new_pte, 0, PGSIZE);
			*pte = PTE_ADDR(new_pte) | PTE_DEF_FLAGS;
		}

		n_start_va = (i == start_idx) ? start_va : cur_va;
		n_end_va = (i == end_idx) ? end_va : cur_va + PDADDR(level, 1) - 1;

		ret = __dune_vm_page_walk((ptent_t *) PTE_ADDR(dir[i]),
					 n_start_va, n_end_va, cb, arg,
					 level - 1, create);
		if (ret)
			return ret;
	}

	return 0;
}

int dune_vm_page_walk(	ptent_t *root, 
						void *start_va, void *end_va,
		     			page_walk_cb cb, const void *arg) {
	return __dune_vm_page_walk(root, start_va, end_va, cb, arg, 3, CREATE_NONE);
}

int dune_vm_lookup(ptent_t *root, void *va, int create, ptent_t **pte_out)
{
	// XXX: Using PA == VA
	int i, j, k, l;
	ptent_t *pml4 = root, *pdpte, *pde, *pte;

	i = PDX(3, va);
	j = PDX(2, va);
	k = PDX(1, va);
	l = PDX(0, va);

	if (!pte_present(pml4[i])) {
		if (!create)
			return -ENOENT;

		pdpte = alloc_page();
		memset(pdpte, 0, PGSIZE);

                pml4[i] = PTE_ADDR(pdpte) | PTE_DEF_FLAGS;
	} else
		pdpte = (ptent_t*) PTE_ADDR(pml4[i]);

	if (!pte_present(pdpte[j])) {
		if (!create)
			return -ENOENT;

		pde = alloc_page();
		memset(pde, 0, PGSIZE);

		pdpte[j] = PTE_ADDR(pde) | PTE_DEF_FLAGS;
	} else if (pte_big(pdpte[j])) {
		*pte_out = &pdpte[j];
		return 0;
	} else
		pde = (ptent_t*) PTE_ADDR(pdpte[j]);

	if (!pte_present(pde[k])) {
		if (!create)
			return -ENOENT;

		pte = alloc_page();
		memset(pte, 0, PGSIZE);

		pde[k] = PTE_ADDR(pte) | PTE_DEF_FLAGS;
	} else if (pte_big(pde[k])) {
		*pte_out = &pde[k];
		return 0;
	} else
		pte = (ptent_t*) PTE_ADDR(pde[k]);

	*pte_out = &pte[l];
	return 0;
}

static int __dune_vm_mprotect_helper(const void *arg, ptent_t *pte, void *va)
{
	ptent_t perm = (ptent_t) arg;

//	if (!(PTE_FLAGS(*pte) & PTE_P))
//		return -ENOMEM;

	*pte = PTE_ADDR(*pte) | (PTE_FLAGS(*pte) & PTE_PS) | perm | PTE_P; 
	return 0;
}

int dune_vm_mprotect(ptent_t *root, void *va, size_t len, int perm)
{
	int ret;
	ptent_t pte_perm;

	if (!(perm & PERM_R)) {
		if (perm & PERM_W)
			return -EINVAL;
		//FIXME: aghosn, this was removing all permissions.
		//perm = PERM_NONE;
	}

	pte_perm = get_pte_perm(perm) | PTE_P;

	ret = __dune_vm_page_walk(root, va, va + len - 1,
				 &__dune_vm_mprotect_helper,
				 (void *) pte_perm, 3, CREATE_NONE);
	if (ret)
		return ret;

	dune_flush_tlb();

	return 0;
}

struct map_phys_data {
	ptent_t perm;
	unsigned long va_base;
	unsigned long pa_base;
};

static int __dune_vm_map_phys_helper(const void *arg, ptent_t *pte, void *va)
{
	struct map_phys_data *data = (struct map_phys_data *) arg;

	*pte = PTE_ADDR(va - data->va_base + data->pa_base) | data->perm | PTE_P;
	return 0;
}

int dune_vm_map_phys(ptent_t *root, void *va, size_t len, void *pa, int perm)
{
	int ret;
	struct map_phys_data data;
	int create;

//	if (!(perm & PERM_R) && (perm & ~(PERM_R)))
//		return -EINVAL;

	data.perm = get_pte_perm(perm);
	data.va_base = (unsigned long) va;
	data.pa_base = (unsigned long) pa;

	if (perm & PERM_BIG)
		create = CREATE_BIG;
	else if (perm & PERM_BIG_1GB)
		create = CREATE_BIG_1GB;
	else
		create = CREATE_NORMAL;

	ret = __dune_vm_page_walk(root, va, va + len - 1,
				 &__dune_vm_map_phys_helper,
				 (void *) &data, 3,
				 create);
	if (ret)
		return ret;

	return 0;
}

static int __dune_vm_map_pages_helper(const void *arg, ptent_t *pte, void *va)
{
	ptent_t perm = (ptent_t) arg;
	struct page *pg = dune_page_alloc();

	if (!pg)
		return -ENOMEM;

	*pte = PTE_ADDR(dune_page2pa(pg)) | perm;

	return 0;
}

int dune_vm_map_pages(ptent_t *root, void *va, size_t len, int perm)
{
	int ret;
	ptent_t pte_perm;

	if (!(perm & PERM_R) && (perm & ~(PERM_R)))
		return -EINVAL;

	pte_perm = get_pte_perm(perm);

	ret = __dune_vm_page_walk(root, va, va + len - 1,
				 &__dune_vm_map_pages_helper,
				 (void *) pte_perm, 3, CREATE_NORMAL);

	return ret;
}

static int __dune_vm_clone_helper(const void *arg, ptent_t *pte, void *va)
{
       int ret;
       struct page *pg = dune_pa2page(PTE_ADDR(*pte));
       ptent_t *newRoot = (ptent_t *)arg;
       ptent_t *newPte;

       ret = dune_vm_lookup(newRoot, va, CREATE_NORMAL, &newPte);
       if (ret < 0)
               return ret;

       if (dune_page_isfrompool(PTE_ADDR(*pte)))
               dune_page_get(pg);
       *newPte = *pte;

       return 0;
}

/**
 * Clone a page root.
 */
ptent_t *dune_vm_clone(ptent_t *root)
{
       int ret;
       ptent_t *newRoot;

       newRoot = alloc_page();
       memset(newRoot, 0, PGSIZE);

       ret = __dune_vm_page_walk(root, VA_START, VA_END,
                       &__dune_vm_clone_helper, newRoot,
                       3, CREATE_NONE);
       if (ret < 0) {
               dune_vm_free(newRoot);
               return NULL;
       }

       return newRoot;
}

static int __dune_vm_free_helper(const void *arg, ptent_t *pte, void *va)
{
	struct page *pg = dune_pa2page(PTE_ADDR(*pte));

	if (dune_page_isfrompool(PTE_ADDR(*pte))) {
		dune_page_put(pg);
	}

	// Invalidate mapping
	*pte = 0;

	return 0;
}

/**
 * Free the page table and decrement the reference count on any pages.
 */
static void dune_vm_free_v2(ptent_t *root)
{
	// XXX: Should only need one page walk
	// XXX: Hacky - Until I fix ref counting
	/*__dune_vm_page_walk(root, VA_START, VA_END,
			&__dune_vm_free_helper, NULL,
			3, CREATE_NONE);*/

	__dune_vm_page_walk(root, VA_START, VA_END,
			&__dune_vm_free_helper, NULL,
			2, CREATE_NONE);

	__dune_vm_page_walk(root, VA_START, VA_END,
			&__dune_vm_free_helper, NULL,
			1, CREATE_NONE);

	if (dune_page_isfrompool((physaddr_t)root))
		put_page(root);
	else
		free(root);

	return;
}

void dune_vm_free(ptent_t *root)
{
	assert(root);

	for (int i = 0; i < NPTENTRIES; i++) {
		if (!pte_present(root[i]))
			continue;

		ptent_t* pdpte = (ptent_t*) PTE_ADDR(root[i]);

		for (int j = 0; j < NPTENTRIES; j++) {
			if (!pte_present(pdpte[j]) || pte_big(pdpte[j]))
				continue;

			/*TODO: handle huge entries.*/
			ptent_t *pde = (ptent_t*) PTE_ADDR(pdpte[j]);

			for (int k = 0; k < NPTENTRIES; k++) {
				/*TODO handle big entries.*/
				if (!pte_present(pde[k]) || pte_big(pde[k]))
					continue;
				
				ptent_t *pte = (ptent_t*) PTE_ADDR(pde[k]);

				for (int l = 0; l < NPTENTRIES; l++) {
					if (!pte_present(pte[l]))
						continue;

					/*Free the entry.*/
					struct page *pg = dune_pa2page(PTE_ADDR(pte[l]));

					if (dune_page_isfrompool(PTE_ADDR(pte[l]))) {
						dune_page_put(pg);
					}
					
					// Invalidate mapping
					pte[l] = 0;
				}

				struct page *pg = dune_pa2page(PTE_ADDR(pde[k]));

				if (dune_page_isfrompool(PTE_ADDR(pde[k]))) {
					dune_page_put(pg);
				}

				pde[k] = 0;
			}

			struct page *pg = dune_pa2page(PTE_ADDR(pdpte[j]));

			if (dune_page_isfrompool(PTE_ADDR(pdpte[j]))) {
				dune_page_put(pg);
			}

			pdpte[j] = 0;
		}

		struct page *pg = dune_pa2page(PTE_ADDR(root[i]));

		if (dune_page_isfrompool(PTE_ADDR(root[i]))) {
			dune_page_put(pg);
		}

		root[i] = 0;
	}

	struct page *pg = dune_pa2page(PTE_ADDR(root));

	if (dune_page_isfrompool(PTE_ADDR(root))) {
		dune_page_put(pg);
	}
}

void dune_vm_unmap(ptent_t *root, void *va, size_t len)
{
	/* FIXME: Doesn't free as much memory as it could */
	__dune_vm_page_walk(root, va, va + len - 1,
			&__dune_vm_free_helper, NULL,
			3, CREATE_NONE);

	dune_flush_tlb();
}


 void dune_vm_default_pgflt_handler(uintptr_t addr, uint64_t fec)
{
	ptent_t *pte = NULL;
	int rc;
	/*
	 * Assert on present and reserved bits.
	 */
	assert(!(fec & (FEC_P | FEC_RSV)));

	rc = dune_vm_lookup(pgroot, (void *) addr, 0, &pte);
	assert(rc == 0);

	if ((fec & FEC_W) && (*pte & PTE_COW)) {
		void *newPage;
		struct page *pg = dune_pa2page(PTE_ADDR(*pte));
		ptent_t perm = PTE_FLAGS(*pte);

		// Compute new permissions
		perm &= ~PTE_COW;
		perm |= PTE_W;

		if (dune_page_isfrompool(PTE_ADDR(*pte)) && pg->ref == 1) {
			*pte = PTE_ADDR(*pte) | perm;
			return;
		}

		// Duplicate page
		newPage = alloc_page();
		memcpy(newPage, (void *)PGADDR(addr), PGSIZE);

		// Map page
		if (dune_page_isfrompool(PTE_ADDR(*pte))) {
			dune_page_put(pg);
		}
		*pte = PTE_ADDR(newPage) | perm;

		// Invalidate
		dune_flush_tlb_one(addr);
	}
}

int dune_vm_has_mapping(ptent_t *root, void *va)
{
	int i, j, k, l;
	ptent_t *pml4 = (ptent_t*) root, *pdpte, *pde, *pte;

	i = PDX(3, va);
	j = PDX(2, va);
	k = PDX(1, va);
	l = PDX(0, va);

	if (!pte_present(pml4[i])) {
		printf("pdpte is Not present.");
		return 4;
	} 

	// printf("The value for i: %d\n", i);
	pdpte = (ptent_t*) PTE_ADDR(pml4[i]);
	// printf("Now pdpte is %p\n", pdpte);

	if (!pte_present(pdpte[j])){
		printf("The pde is not present!\n");
		printf("0x%016lx\n", pdpte[j]);
		return 3;
	}


	// printf("The pde is present.\n");
	if (pte_big(pdpte[j])) {
		// printf("Using the big scheme.\n");
		return 0;
	}

	pde = (ptent_t*) PTE_ADDR(pdpte[j]);
	// printf("The pde %p \n", pde);

	if (!pte_present(pde[k])) {
		printf("The pte is not present.\n");
		printf("The pte is 0x%016lx\n", pde[k]);
		return 2;
	}

	if (pte_big(pde[k])) {
		// printf("Using the big scheme.\n");
		return 0;
	}

	pte = (ptent_t *) PTE_ADDR(pde[k]);
	// printf("pte is present and is %p.\n", pte);
	
	//ptent_t* out = &pte[l];

	if (!pte_present(pte[l])) {
		printf("The missing pte 0x%016lx\n", pte[l]);
		return 1;
	}
	
	return 0;
}


int vm_compare_mappings(ptent_t *first, ptent_t *second)
{
	int ret = 0;
	ptent_t *f_pml4 = first, *f_pdpte, *f_pde, *f_pte;
	ptent_t *s_pml4 = second, *s_pdpte, *s_pde, *s_pte;

	for (int i = 0; i < NPTENTRIES; i++) {
		if (!pte_present(f_pml4[i]) && !pte_present(s_pml4[i]))
			continue;
		if (pte_present(f_pml4[i]) != pte_present(s_pml4[i]))
			return 1;

		if (PPTE_FLAGS(f_pml4[i]) != PPTE_FLAGS(s_pml4[i]))
			return 2;

		f_pdpte = (ptent_t *) PTE_ADDR(f_pml4[i]);
		s_pdpte = (ptent_t *) PTE_ADDR(s_pml4[i]);

		for (int j = 0; j < NPTENTRIES; j++) {
			if (!pte_present(f_pdpte[j]) && !pte_present(s_pdpte[j]))
				continue;

			if (pte_present(f_pdpte[j]) != pte_present(s_pdpte[j])) {
				return 3;
			}

			if (PPTE_FLAGS(f_pdpte[j]) != PPTE_FLAGS(s_pdpte[j]))
				return 4;

			if (pte_big(f_pdpte[j]) && pte_big(s_pdpte[j]))
				continue;

			if (pte_big(f_pdpte[j]) != pte_big(s_pdpte[j]))
				return 5;

			f_pde = (ptent_t *)PTE_ADDR(f_pdpte[j]);
			s_pde = (ptent_t *)PTE_ADDR(s_pdpte[j]);
			for (int k = 0; k < NPTENTRIES; k++) {
				if (!pte_present(f_pde[k]) && !pte_present(s_pde[k]))
					continue;

				if ((pte_present(f_pde[k]) != pte_present(s_pde[k])))
					return 6;

				//TODO check flags.
				if (PPTE_FLAGS(f_pde[k]) != PPTE_FLAGS(s_pde[k]))
					return 7;

				if (pte_big(f_pde[k]) && pte_big(s_pde[k]))
					continue;

				if (pte_big(f_pde[k]) != pte_big(s_pde[k]))
					return 8;

				f_pte = (ptent_t*)PTE_ADDR(f_pde[k]);
				s_pte = (ptent_t*)PTE_ADDR(s_pde[k]);

				for (int l = 0; l < NPTENTRIES; l++) {
					if (!pte_present(f_pte[l]) && !pte_present(s_pte[l]))
						continue;

					//TODO compare flags;
					if (pte_present(f_pte[l]) != pte_present(s_pte[l])) {
						printf("%d, %d, %d, %d\n", i, j, k, l);
						printf("0x%016lx\n", RPDX(i, j, k, l));
						printf("First %d second %d\n", pte_present(f_pte[l]), pte_present(s_pte[l]));
						return 9;
					}

					if (PPTE_FLAGS(f_pte[l]) != PPTE_FLAGS(s_pte[l])) {
						printf("0x%016lx \n", RPDX(i, j, k, l));
						printf("Flags: 0x%016lx and 0x%016lx\n", PTE_FLAGS(f_pte[l]), PTE_FLAGS(s_pte[l]));
						return 10;
					}
				}
			}
		}
	}

	return ret;
}

static int __vm_make_root(const void* arg, ptent_t *pte, void* va)
{
	*pte &= ~(PTE_U);
	return 0; 
}

int vm_make_root(void* va_start, void* va_end, ptent_t* root)
{
	return dune_vm_page_walk(root, va_start, va_end -1, &__vm_make_root , NULL);
}

typedef struct test_result {
	uint64_t nb_root;
	uint64_t nb_user;
} test_result;

static int __vm_count_entries(const void* arg, ptent_t* pte, void *va)
{
	test_result *res = (test_result*) (arg);
	if (*pte & PTE_U)
		res->nb_user++;
	else 
		res->nb_root++;
	return 0;
}

int vm_count_entries(void* va_start, void* va_end, ptent_t *root)
{
	test_result res = {0,0};
	dune_vm_page_walk(root, va_start, va_end-1, &__vm_count_entries, &res);
	printf("NB ROOT %lu\n", res.nb_root);
	printf("NB USER %lu\n", res.nb_user);
	return 0;
}

int __vm_check_entries(const void *arg, ptent_t *pte, void *va)
{
	unsigned long flags = *((unsigned long*) arg);
	ptent_t perm = get_pte_perm(flags);
	if (((PTE_ADDR(*pte) ^ *pte) ^ perm)) {
		printf("pte: 0x%016lx and perm 0x%016lx\n", (PTE_ADDR(*pte)^ *pte), perm);
		return 1;
	}
	return 0;
}

int vm_check_entry(void *start, void *end, ptent_t* root, unsigned long flags)
{
	return dune_vm_page_walk(root, start, end, &__vm_check_entries, &flags);
}
