#ifndef __LIBDUNE_MM_MM_TYPES_H__
#define __LIBDUNE_MM_MM_TYPES_H__

#include "../dune.h"

typedef unsigned long vm_addrptr;

/* Defines a list of vm_area_struct*/
TAILQ_HEAD(vm_area_list, vm_area_struct);

typedef struct vm_area_struct {

	/* Address of the first byte within the vma.*/
	vm_addrptr vm_start;
	/* Address of the first byte after our end address*/
	vm_addrptr vm_end;

	/* Linked list of VM areas, sorted by address*/
	TAILQ_ENTRY(vm_area_struct) q_areas;

	//TODO: implement red-black tree.
	
	/* Address space we belong to*/
	struct mm_struct *vm_mm;
	
	/*Flags, see libdune/mm.h*/
	unsigned long vm_flags;

	/* Flags vma modified but not applied to pgroot.*/
	unsigned int dirty	: 1;
	
} vm_area_struct;

typedef struct mm_struct {
	struct vm_area_list mmap;

	ptent_t* pml4;

	TAILQ_ENTRY(mm_struct) q_mms;

	unsigned int ref;
} mm_struct;

/* List of memory regions*/
TAILQ_HEAD(mm_list, mm_struct);

#endif