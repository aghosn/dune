#ifndef __LIBDUNE_MM_VMA_H__
#define __LIBDUNE_MM_VMA_H__

#include <stdlib.h>
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

/* Create a new vma.
 * WARNING: does not add the vma to the mm struct list.*/
vm_area_struct *vma_create(	mm_struct *mm,
							vm_addrptr start,
							vm_addrptr end,
							unsigned long perm);

/* Free a vma.
 * WARNING: does not modifiy the page table mappings.*/
int vma_free(vm_area_struct *vma);

/* Perform a copy of a vma. If second argument is true, dirty bit set to 1.*/
vm_area_struct *vma_copy(vm_area_struct *vma, bool cow);

vm_area_struct *vma_cow_copy(vm_area_struct *vma);
vm_area_struct *vma_shared_copy(vm_area_struct *vma);
void vma_dump(vm_area_struct *vma);
#endif