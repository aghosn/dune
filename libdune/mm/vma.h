#ifndef __LIBDUNE_MM_VMA_H__
#define __LIBDUNE_MM_VMA_H__
#include <stdbool.h>

#include "mm_types.h"

/* Macros to handle page alignment*/
#define MM_PGALIGN_DN(addr) ((addr) & ~(PGSIZE - 1))
#define MM_PGALIGN_UP(addr) (((addr) + (PGSIZE-1)) & ~(PGSIZE - 1))

/* Helper functions*/

static inline bool vma_is_user(vm_area_struct *vma)
{
	ASSERT_DBG(vma, "vma is null.\n");
	return (vma->vm_flags & PERM_U);
}

/* API*/
vm_area_struct *vma_create(	mm_struct *mm,
							vm_addrptr start,
							vm_addrptr end,
							unsigned long perm);

int vma_free(vm_area_struct *vma);

vm_area_struct *vma_copy(vm_area_struct *vma, bool cow);

//TODO: move into the vma.c
vm_area_struct *vma_cow_copy(vm_area_struct *vma);
vm_area_struct *vma_shared_copy(vm_area_struct *vma);
void vma_dump(vm_area_struct *vma);
#endif /*__LIBDUNE_MM_VMA_H__*/