#ifndef __LIBDUNE_MM_MM_TYPES_H__
#define __LIBDUNE_MM_MM_TYPES_H__

#include "../dune.h"
#include "../utils/vq.h"

typedef unsigned long vm_addrptr;

typedef struct vm_area_struct {
	vm_addrptr vm_start;	/*Start address within the vm_mm*/
	vm_addrptr vm_end;	/*First byte after our end address*/

	/*Linked list of VM areas, should be sorted by address*/
	Q_NEW_LINK(vm_area_struct) lk_areas;
	//TODO tree at some point.
	
	struct mm_struct *vm_mm;	/*Address space we belong to*/
	
	//TODO: equivalent to pgrot. Should add COW to it.
	unsigned long vm_flags; /*Flags, see libdune/mm.h*/

	unsigned int user : 1; /*Dune specific, 1 -> user, 0 -> supervisor*/
} vm_area_struct;

/*Define a list of vm_area_struct*/
Q_NEW_HEAD(l_vm_area, vm_area_struct)

typedef struct mm_struct {
	l_vm_area *mmap;

	ptent_t* pml4;

	Q_NEW_LINK(mm_struct) lk_mms;

	unsigned int ref;	/*Reference counter. Should be atomically modified*/
} mm_struct;

/*List of memory regions*/
Q_NEW_HEAD(l_mm, mm_struct)

#endif