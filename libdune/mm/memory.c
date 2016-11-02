#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <stdbool.h>


#include "memory.h"

/*Global variables*/
ptent_t *pgroot = NULL;
mm_struct *mm_root = NULL;
l_mm *mm_queue = NULL;


int dune_memory_init(bool map_full) {
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
	mm_root->mmap = malloc(sizeof(l_vm_area));

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
	Q_INSERT_FRONT(mm_queue, mm_root,  lk_mms);

	if (ret = dune_page_init()) {
		printf("dune: unable to initialize page manager.\n");
		goto err;
	}

	if (ret = mm_init(map_full)) {
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
