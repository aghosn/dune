#ifndef __LIBDUNE_MM_MM_TOOLS_H__
#define __LIBDUNE_MM_MM_TOOLS_H__

#include <stdbool.h>
#include "mm_types.h"

/* Call back function type.*/
typedef int (*mm_cb_ft)(vm_area_struct*, void*);

/* Enables to copy the memory mappings.
 * Caller has the choice to apply the changes to pgroot.
 * If cow is true, than user regions are copy on write, otherwise,
 * they are shared.*/
mm_struct* mm_copy(mm_struct *mm, bool apply, bool cow);

/* Checks if the given vma overlaps with the given range of addresses.*/
int mm_overlap(vm_area_struct *vma, vm_addrptr s, vm_addrptr e);

/* Handles the splitting of vmas.*/
int mm_split_and_merge(	mm_struct *mm,
						vm_area_struct *vma, 
						vm_addrptr start,
						vm_addrptr end,
						unsigned long perm,
						mm_cb_ft f,
						void *args);

int mm_split_no_merge(	mm_struct *mm,
						vm_area_struct *vma,
						vm_addrptr start,
						vm_addrptr end,
						unsigned long perm,
						mm_cb_ft f,
						void *args);

/* Deletes the defined region from the memory mappings.*/
int mm_delete_region(	mm_struct *mm,
						vm_area_struct *start,
						vm_area_struct *end);

/* Frees the memory mappings.*/
int mm_free(mm_struct *mm);

/* Uncows a mapping.*/
void mm_uncow(mm_struct *mm, vm_addrptr va);

int mm_verify_range(mm_struct *mm, vm_addrptr addr, uint64_t len);

/* Pretty prints the mm regions.*/
void mm_dump(mm_struct *mm);

/* Helper function to verify that regions correspond to entries inside pgrot.*/
int mm_verify_mappings(mm_struct *mm);

/* Compares two memory regions.*/
int mm_assert_equals(mm_struct *o, mm_struct *c);
#endif /*__LIBDUNE_MM_MM_TOOLS_H__*/