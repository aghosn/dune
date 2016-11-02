#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>
#include <errno.h>
#include <sys/ioctl.h>

#include "../dune.h"
#include "mm.h"
#include "mm_types.h"
#include "memory.h"

/*Global variables for memory limits.*/
uintptr_t phys_limit;
uintptr_t mmap_base;
uintptr_t stack_base;
int dune_fd;

//TODO change the permissions to use our own.
int mm_init(bool map_full) {
	int ret;
	void *start = NULL, *end = NULL, *pa = NULL;
	unsigned long perm = 0;
	struct dune_layout layout;
	
	if (ret = ioctl(dune_fd, DUNE_GET_LAYOUT, &layout))
	 	return ret;
	
	phys_limit = layout.phys_limit;
	mmap_base = layout.base_map;
	stack_base = layout.base_stack;

	if (map_full) {
		start = (void*) 0;
		end = (void*) (1UL << 32);
		pa = (void*) 0;
		perm = PERM_R | PERM_W | PERM_X | PERM_U;
	} else {
		start = (void*) PAGEBASE;
		end = (void*) (PAGEBASE + MAX_PAGES * PGSIZE);
		pa = (void*) dune_va_to_pa((void*)PAGEBASE);
		perm = PERM_R | PERM_W | PERM_BIG;
	}

	if ((ret = mm_create_mapping(mm_root,(vm_addrptr) start,(vm_addrptr) end, pa, perm)))
		return ret;

	return 0;
}

int mm_create_mapping(	mm_struct *mm, 
						vm_addrptr va_start, 
						vm_addrptr va_end, 
						void* pa, 
						unsigned long perm) {
	vm_area_struct *current = NULL, *vma = NULL;
	Q_FOREACH(current, mm->mmap, lk_areas) {
		if (current->vm_start == va_start &&
			current->vm_end == va_end &&
			current->vm_flags == perm) {
			vma = current;
			break;
		}

		if (mm_overlap(current, va_start, va_end, perm)) {
			vma = mm_split_or_merge(current, va_start, va_end, perm);
			break;
		}
	}
	if (vma)
		return 0;
	/*Must create it now.*/
	vma = malloc(sizeof(vm_area_struct));
	if (!vma)
		return -ENOMEM;

	Q_INIT_ELEM(vma, lk_areas);
	vma->vm_start = va_start;
	vma->vm_end = va_end;
	vma->vm_mm = mm; 
	vma->vm_flags = perm;	//TODO change permissions.

	//TODO actually map the pages.
	
	Q_INSERT_TAIL(mm->mmap, vma, lk_areas);
	return 0;
}