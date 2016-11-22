#ifndef __LIBDUNE_MM_MM_H__
#define __LIBDUNE_MM_MM_H__
#include <stdbool.h>

#include "vma.h"
#include "mm_types.h"
#include "mm_tools.h"

/* The memory mappings API*/
int mm_init();
int mm_create_phys_mapping(	mm_struct *mm, 
							vm_addrptr start, 
							vm_addrptr end, 
							void* pa, 
							unsigned long perm);

int mm_apply_to_pgroot(vm_area_struct *vma, void *pa);

int mm_mprotect(mm_struct *mm, vm_addrptr start,
				vm_addrptr end, unsigned long perm);

int mm_unmap(mm_struct *mm, vm_addrptr start, vm_addrptr end, bool apply);

void mm_apply(mm_struct *mm);

#endif /*__LIBDUNE_MM_MM_H__*/