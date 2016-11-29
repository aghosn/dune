#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <stdbool.h>


#include "memory.h"
#include "vm_tools.h"

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
	
	dune_register_pgflt_handler((dune_pgflt_cb)&memory_pgflt_handler);

	if ((ret = mm_init())) {
		printf("dune: unable to setup memory layout.\n");
		goto err;
	}

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

void memory_pgflt_handler(uintptr_t addr, uint64_t fec)
{
	assert(fec & FEC_W);
	/* Check that everything is set properly*/
	mm_struct *current_mm = memory_get_mm();
	assert(current_mm->pml4 == pgroot);
	mm_verify_mappings(current_mm);
	
	/* Do the uncow for the memory region.*/
	mm_uncow(current_mm, (vm_addrptr) addr);
}