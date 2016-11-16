#ifndef __LIBDUNE_MM_VMA_H__
#define __LIBDUNE_MM_VMA_H__

#include "mm_types.h"

/* Macros to handle page alignment*/
#define MM_PGALIGN_DN(addr) ((addr) & ~(PGSIZE - 1))
#define MM_PGALIGN_UP(addr) (((addr) + (PGSIZE-1)) & ~(PGSIZE - 1))

vm_area_struct *vma_create(	mm_struct *mm,
							vm_addrptr start,
							vm_addrptr end,
							unsigned long perm);

#endif