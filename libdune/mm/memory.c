#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <stdbool.h>


#include "memory.h"

/*Global variables*/
ptent_t *pgroot = NULL;
mm_struct *mm_root = NULL;
l_mm *mm_queue = NULL;


int dune_memory_init() {
	int ret;
	pgroot = memalign(PGSIZE, PGSIZE);
	if (!pgroot) {
		ret = -ENOMEM;
		goto err;
	}
	memset(pgroot, 0, PGSIZE);

	mm_root = malloc(sizeof(mm_struct));
	if (!mm_root) {
		ret = -ENOMEM;
		goto err;
	}
	Q_INIT_ELEM(mm_root, lk_mms);
	mm_root->pml4 = pgroot;
	mm_root->ref = 1;
	mm_root->mmap = malloc(sizeof(l_vm_area));
	Q_INIT_HEAD(mm_root->mmap);

	if(!mm_root->mmap) {
		ret = -ENOMEM;
		goto err;
	}

	mm_queue = malloc(sizeof(l_mm));
	if (!mm_queue) {
		ret = -ENOMEM;
		goto err;
	}
	Q_INIT_HEAD(mm_queue);
	Q_INSERT_FRONT(mm_queue, mm_root, lk_mms);

	if ((ret = dune_page_init())) {
		printf("dune: unable to initialize page manager.\n");
		goto err;
	}
	
	if ((ret = mm_init())) {
		printf("dune: unable to setup memory layout.\n");
		goto err;
	}

	dune_register_pgflt_handler((dune_pgflt_cb)&memory_default_pgflt_handler);

	return 0;
err:
	if (pgroot)
		free(pgroot);
	if (mm_root && mm_root->mmap)
		free(mm_root->mmap);
	if (mm_root)
		free(mm_root);
	if (mm_queue)
		free(mm_queue);
	return ret;
}

void memory_default_pgflt_handler(uintptr_t addr, uint64_t fec)
{
	ptent_t *pte = NULL;
	int rc;
	/*
	 * Assert on present and reserved bits.
	 */
	printf("The fec %lu and address %p\n", fec, addr);
	rc = dune_vm_lookup(pgroot, (void *) addr, 0, &pte);
	
	fflush(stdout);
	assert(rc == 0);
	assert(*pte & PTE_U);
	
	// if ((*pte & PTE_U) && (*pte & PTE_COW))
	// 	printf("Looks good to me for the moment.\n");
	// else
	// 	printf("Shhhhhoooot\n");

	if ((fec & FEC_W) && (*pte & PTE_COW)) {
		void *newPage;
		struct page *pg = dune_pa2page(PTE_ADDR(*pte));
		ptent_t perm = PTE_FLAGS(*pte);

		// Compute new permissions
		perm &= ~PTE_COW;
		perm |= PTE_W;

		if (dune_page_isfrompool(PTE_ADDR(*pte)) && pg->ref == 1) {
			fflush(stdout);
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
	printf("And solved.\n");
}
