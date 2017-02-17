#ifndef __LIBDUNE_MM_MEMORY_H__
#define __LIBDUNE_MM_MEMORY_H__

#include <stdbool.h>
#include <sys/queue.h>

#include "../dune.h"
#include "mm.h"
#include "vma.h"
#include "mm_types.h"
#include "vm.h"
#include "page.h"


/*Global variables.*/

/* The current pageroot mapping.
 * FIXME: keep it for backwards compatibility with dune.*/
extern ptent_t *pgroot;

/* The original memory mapping.*/
extern mm_struct *mm_root;

/* All the memory mappings created.
 * The current one is at the head.*/
extern struct mm_list *mm_queue;

/* Functions for safe access to global variables.*/

/* Returns the current memory mapping.*/
static inline mm_struct* memory_get_mm()
{
	mm_struct *current = TAILQ_FIRST(mm_queue);
	ASSERT_DBG(current && current->pml4, "mm not initilized.\n");
	
	ASSERT_DBG(current->pml4 == pgroot,
		"current->pml4{%p}, pgroot{%p}.\n", current->pml4, pgroot);

	return current;
}

/* Returns the current page root.*/
static inline ptent_t* memory_get_pgrot() {
	mm_struct *current = TAILQ_FIRST(mm_queue);
	ASSERT_DBG(current && current->pml4, "mm not initilized.\n");
	
	ASSERT_DBG(current->pml4 == pgroot,
		"current->pml4{%p}, pgroot{%p}.\n", current->pml4, pgroot);

	return current->pml4;
}

/* Change the current memory mapping*/
static inline void memory_switch(mm_struct *mm) {
	ASSERT_DBG(mm, "mm is null.\n");
	ASSERT_DBG(mm->pml4, "mm->pml4 is null.\n");

	mm_struct *current = TAILQ_FIRST(mm_queue);
	
	ASSERT_DBG(pgroot == current->pml4,
		"current->pml4{%p}, pgroot{%p}.\n", current->pml4, pgroot);

	TAILQ_REMOVE(mm_queue, mm, q_mms);
	TAILQ_INSERT_HEAD(mm_queue, mm, q_mms);
	pgroot = mm->pml4;
	load_cr3((unsigned long)(mm->pml4));
} 


int dune_memory_init();
void memory_pgflt_handler(uintptr_t addr, uint64_t fec, struct dune_tf *tf);
#endif /*__LIBDUNE_MM_MEMORY_H__*/