#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <malloc.h>
#include <stdbool.h>

#include "memory.h"
#include "vm_tools.h"
#include "../local.h"

/*Global variables*/
ptent_t *pgroot = NULL;
mm_struct *mm_root = NULL;
struct mm_list *mm_queue = NULL;


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
	
	mm_root->pml4 = pgroot;
	mm_root->ref = 1;
	TAILQ_INIT(&(mm_root->mmap));

	mm_queue = malloc(sizeof(struct mm_list));
	if (!mm_queue) {
		ret = -ENOMEM;
		goto err;
	}
	TAILQ_INIT(mm_queue);
	TAILQ_INSERT_HEAD(mm_queue, mm_root, q_mms);

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
	if (mm_root)
		free(mm_root);
	if (mm_queue)
		free(mm_queue);
	return ret;
}

static uint64_t read_cr3() {
    uint64_t cr3 = 0xdeadbeef;
    __asm__ __volatile__ (
        "mov %%cr3, %%rax\n\t"
        "mov %%rax, %0\n\t"
        : "=m" (cr3)
        : /* no input */
        : "%rax"
    );

    return cr3;
}

void memory_pgflt_handler(uintptr_t addr, uint64_t fec, struct dune_tf *tf)
{
	/* We handle only COW page faults.*/
	ASSERT_DBG(fec & FEC_W, " addr{0x%016lx}, fec{%lx}: Illegal read.\n", addr, fec);
	
	/* Check that everything is set properly*/
	mm_struct *current_mm = memory_get_mm();
	ASSERT_DBG(	current_mm->pml4 == pgroot,
				"current_mm{0x%016lx}, pgroot{0x%016lx}",
				(unsigned long)current_mm->pml4,
				(unsigned long)pgroot);

	ptent_t* cr3 = (ptent_t*)read_cr3();
	ASSERT_DBG(pgroot == cr3, "pgroot{0x%016lx}, cr3{0x%016lx}", 
			(unsigned long)pgroot, (unsigned long)cr3);

	mm_verify_mappings(current_mm);
	/* Do the uncow for the memory region.*/
	mm_uncow(current_mm, (vm_addrptr) addr);
}