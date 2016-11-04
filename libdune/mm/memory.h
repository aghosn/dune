#ifndef __LIBDUNE_MM_MEMORY_H__
#define __LIBDUNE_MM_MEMORY_H__

#include <stdbool.h>

#include "../dune.h"
#include "mm.h"
#include "mm_types.h"
#include "vm.h"
#include "page.h"

/*Global variables.*/
extern ptent_t *pgroot;
extern mm_struct *mm_root;
extern l_mm *mm_queue;

int dune_memory_init();
#endif /*__LIBDUNE_MM_MEMORY_H__*/