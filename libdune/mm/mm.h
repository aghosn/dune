#ifndef __LIBDUNE_MM_MM_H__
#define __LIBDUNE_MM_MM_H__
#include <stdbool.h>

#include "vma.h"
#include "mm_types.h"
#include "mm_tools.h"

/* The memory mappings API*/
int mm_init();

/* Find the vma containing the address.
 * If no vma contains the given addr, it returns the vma that comes before that
 * or the first vma if there is no predecessor.*/
vm_area_struct* mm_find(mm_struct *mm, vm_addrptr addr, bool is_end);

int mm_create_phys_mapping(	mm_struct *mm, 
							vm_addrptr start, 
							vm_addrptr end, 
							void* pa, 
							unsigned long perm);

int mm_apply_to_pgroot(vm_area_struct *vma, void *pa);

int mm_mprotect(mm_struct *mm, vm_addrptr start,
				vm_addrptr end, unsigned long perm);

int mm_unmap(mm_struct *mm, vm_addrptr start, vm_addrptr end, bool apply);

/* Applies mappings to pageroot for user entries marked as dirty.*/
void mm_apply(mm_struct *mm);

#endif /*__LIBDUNE_MM_MM_H__*/