#ifndef __LWC_LIBLWC_VM_MM_TYPES_H__
#define __LWC_LIBLWC_VM_MM_TYPES_H__

#include "../tools/vq.h"

typedef struct vm_area_t {
	/*Limits for the vm_area*/
	uint64_t vm_start;
	uint64_t vm_end;

	/*Access rights as defined in TODO*/
	unsigned long vm_flags;

	/*Maintainance*/
	uint64_t ref;					/*Reference counter*/
	Q_NEW_LINK(vm_area_t) lk_vmas;	/*List of all vmas*/
} vm_area_t;

/*Define a list of vm_areas as l_vm_areas_t*/
Q_NEW_HEAD(l_vm_areas_t, vm_area_t);

/**
 * Wrapper for a vma, that enables to put it in several lists.
 * Whenever a vma is used inside an lwc, it should create a wrapper,
 * increment the vma->ref.
 * When the wrapper is destroyed, decrement vma->ref.
 * If an operation must modify the vma, and ref != 1, it must create a new one.
 */
typedef struct wrap_vm_area_t {
	vm_area_t 	*vm_area;

	/*Maintainance*/
	Q_NEW_LINK(vm_area_t) lk_lwc;	/*List of regions within lwc.*/
} wrap_vm_area_t;

/*Define a list of vma wrappers as l_wrap_vm_areas.*/
Q_NEW_HEAD(l_wrap_vm_areas_t, wrap_vm_area_t);

/**
 * Represents a memory mapping, similar to linux/mm_types.h:mm_struct.
 * It contains a list of the the vmas it maps,
 * the pageroot that corresponds to this mapping,
 * as well as some bookkeeping informations.
 */
typedef struct {
	/*The VMA list.*/
	l_wrap_vm_areas_t mmap;

	/*Actual page root*/
	ptent_t	*pgroot;
} mm_t;


#endif /*__LWC_LIBLWC_VM_MM_TYPES_H__*/