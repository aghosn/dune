#ifndef __LIBDUNE_MM_MEMORY_H__
#define __LIBDUNE_MM_MEMORY_H__

#include <stdbool.h>

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

/* The original memory mapping.
 * TODO: should we really keep that around?*/
extern mm_struct *mm_root;

/* All the memory mappings created.
 * The current one is at the head.*/
extern l_mm *mm_queue;

/* Functions for safe access to global variables.*/

/* Returns the current memory mapping.*/
static inline mm_struct* memory_get_mm()
{
	mm_struct *current = Q_GET_FRONT(mm_queue);
	assert(current && current->pml4);
	assert(current->pml4 == pgroot);
	return current;
}

/* Returns the current page root.*/
static inline ptent_t* memory_get_pgrot() {
	mm_struct *current = Q_GET_FRONT(mm_queue);
	assert(current && current->pml4);
	assert(current->pml4 == pgroot);
	return current->pml4;
} 


int dune_memory_init();
void memory_pgflt_handler(uintptr_t addr, uint64_t fec);
#endif /*__LIBDUNE_MM_MEMORY_H__*/