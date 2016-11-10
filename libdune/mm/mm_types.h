#ifndef __LIBDUNE_MM_MM_TYPES_H__
#define __LIBDUNE_MM_MM_TYPES_H__

#include "../dune.h"
#include "../utils/vq.h"

typedef unsigned long vm_addrptr;

/*Defines a list of vm_area_struct*/
Q_NEW_HEAD(l_vm_area, vm_area_struct)

typedef struct vm_area_struct {

	/* Address of the first byte within the vma.*/
	vm_addrptr vm_start;
	/* Address of the first byte after our end address*/
	vm_addrptr vm_end;

	/*Linked list of VM areas, should be sorted by address*/
	Q_NEW_LINK(vm_area_struct) lk_areas;
	
	//TODO: implement red-black tree.
	
	/* Address space we belong to*/
	struct mm_struct *vm_mm;
	
	/*Flags, see libdune/mm.h*/
	unsigned long vm_flags;

	/* Dune specific, 1 -> user, 0 -> supervisor*/
	unsigned int user	: 1;
	/* Flags vma modified but not applied to pgroot.*/
	unsigned int dirty	: 1;
	/* VMA is copy on write.*/
	unsigned int cow 	: 1;
	/* VMA shares page mappings with other vmas.*/
	unsigned int shared	: 1;

	/* Vmas that share the same page mappings or are copy-on-write.*/
	l_vm_area *head_shared;
	Q_NEW_LINK(vm_area_struct) lk_shared;
} vm_area_struct;

typedef struct mm_struct {
	l_vm_area *mmap;

	ptent_t* pml4;

	Q_NEW_LINK(mm_struct) lk_mms;

	unsigned int ref;	/*Reference counter. Should be atomically modified*/
} mm_struct;

/*List of memory regions*/
Q_NEW_HEAD(l_mm, mm_struct)

#endif