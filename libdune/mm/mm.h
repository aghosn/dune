#ifndef __LIBDUNE_MM_MM_H__
#define __LIBDUNE_MM_MM_H__
#include <stdbool.h>

#include "mm_types.h"

/* Macros to handle page alignment*/
#define MM_PGALIGN_DN(addr) ((addr) & ~(PGSIZE - 1))
#define MM_PGALIGN_UP(addr) (((addr) + (PGSIZE-1)) & ~(PGSIZE - 1))

/* Call back function type.*/
typedef int (*mm_cb_ft)(vm_area_struct*, void*);

/* The memory mappings API*/
int mm_init();
int mm_create_phys_mapping(	mm_struct *mm, 
							vm_addrptr start, 
							vm_addrptr end, 
							void* pa, 
							unsigned long perm);

int mm_overlap(vm_area_struct *vma, vm_addrptr s, vm_addrptr e);

int mm_split_or_merge(	mm_struct *mm,
						vm_area_struct *vma, 
						vm_addrptr start,
						vm_addrptr end,
						unsigned long perm,
						mm_cb_ft f,
						void * args);

int mm_apply_to_pgroot(vm_area_struct *vma, void *pa);
void mm_dump(mm_struct *mm);
int mm_mprotect(mm_struct *mm, vm_addrptr start,
				vm_addrptr end, unsigned long perm);
int mm_apply_protect(vm_area_struct *vma, void* perm);

#endif /*__LIBDUNE_MM_MM_H__*/