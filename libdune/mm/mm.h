#ifndef __LIBDUNE_MM_MM_H__
#define __LIBDUNE_MM_MM_H__
#include <stdbool.h>

#include "mm_types.h"

/*Global variables for memory limits.*/
extern uintptr_t phys_limit;
extern uintptr_t mmap_base;
extern uintptr_t stack_base;

int mm_init(bool map_full);
int mm_create_mapping(	mm_struct *mm, 
						vm_addrptr start, 
						vm_addrptr end, 
						void* pa, 
						unsigned long perm);

int mm_overlap(vm_area_struct *vma, vm_addrptr s, vm_addrptr e);

vm_area_struct* mm_split_or_merge(	mm_struct *mm,
									vm_area_struct *vma, 
									vm_addrptr start, 
									vm_addrptr end, 
									unsigned long perm);

int mm_apply_to_pgroot(vm_area_struct *vma);
#endif /*__LIBDUNE_MM_MM_H__*/