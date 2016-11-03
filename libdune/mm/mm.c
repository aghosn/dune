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

/* Allocates and initializes a vm_area.
 * WARNING: it does not add it to mm.
 * WARNING: it does not change the pgroot mappings either.*/
static vm_area_struct * mm_alloc_vma(mm_struct *mm,
									vm_addrptr start,
									vm_addrptr end,
									unsigned long perm)
{
	vm_area_struct *vma = malloc(sizeof(vm_area_struct));
	if (!vma)
		return NULL;
	vma->vm_start = start;
	vma->vm_end = end;
	vma->vm_flags = perm;

	vma->vm_mm = mm;
	Q_INIT_ELEM(vma, lk_areas);
	return vma;
}

/* Removes the vmas between start (not included) and end (not included) for the mm->mmap.
 * Frees the removed vmas.
 * If start == NULL start at head.
 * If end == NULL ends after last.
 * WARNING: does not affect the page tables.*/
static int mm_delete_region(mm_struct *mm,
							vm_area_struct *start,
							vm_area_struct *end)
{
	vm_area_struct *iter = (!start)? mm->mmap->head : start->lk_areas.next;
	while (iter != NULL) {
		vm_area_struct *tmp = iter;
		iter = iter->lk_areas.next;
		
		Q_REMOVE(mm->mmap, tmp, lk_areas);
		free(tmp);
		if (iter == end)
			break;
	}

	return 0;
}

//TODO change the permissions to use our own.
int mm_init(bool map_full)
{
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
						unsigned long perm)
{
	vm_area_struct *current = NULL, *vma = NULL;
	Q_FOREACH(current, mm->mmap, lk_areas) {
		if (current->vm_start == va_start &&
			current->vm_end == va_end &&
			current->vm_flags == perm) {
			vma = current;
			break;
		}

		if (mm_overlap(current, va_start, va_end)) {
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


int mm_overlap(vm_area_struct *vma, vm_addrptr start, vm_addrptr end)
{
	int dont_overlap = (vma->vm_end) < (start) || (vma->vm_start) >(end);
	return !(dont_overlap);
}

int mm_split_or_merge(	mm_struct *mm,
						vm_area_struct *vma,
						vm_addrptr start,
						vm_addrptr end,
						unsigned long perm)
{
	assert(vma);
	assert(start <= end);
	vm_area_struct *iter = NULL, vm_area_struct *last = vma;
	
	/* Find the last conflicting block.*/
	for (iter = vma->lk_areas.next; iter != NULL; iter = iter->lk_areas.next) {
		if (!mm_overlap(iter, start, end)) {
			last = iter->lk_areas.prev;
			break;
		}
	}
	
	/*1. creates only one vma, it is a merge 
	 *2. creates only two blocks.
	 *3. creates three blocks.*/

	/*1.*/
	if (start <= vma->vm_start && end >= last->vm_end) {
		vm_area_struct *nvma = mm_alloc_vma(vma->vm_mm, start, end, perm);
		if (!nvma)
			return NULL;
		//TODO clean up the ones in-between.
		//call the pgroot functions.
		return 0;
	}

	/*2. TODO move the first up and handle better.*/
	vm_addrptr f_start = (vma->vm_start < start)? vma->vm_start : start;
	vm_addrptr f_end = (vma->vm_start > start)? vma->vm_start : start;
	unsigned long f_perm = (vma->vm_start < start)? vma->vm_flags : perm;
	
	vm_area_struct *f_vma = mm_alloc_vma(vma->vm_mm, f_start, f_end, f_perm);
	if (!f_vma)
		goto err;

	vm_addrptr s_start = f_end;
	vm_addrptr s_end = (vma->vm_end <= end)? vma->vm_end : end;
	unsigned long s_perm = (f_perm == perm)? vma->vm_flags : perm;

	vm_area_struct *s_vma = mm_alloc_vma(vma->vm_mm, s_start, s_end, s_perm);
	if (!s_vma)
		goto err;

	/*3.*/
	vm_area_struct *t_vma = NULL;
	vm_addrptr max_end = (vma->vm_end < end)? end : vma->vm_end;
	if (s_end != max_end) {
		vm_addrptr t_start = s_end;
		vm_addrptr t_end = max_end;
		unsigned long t_perm = f_perm;

		t_vma = mm_alloc_vma(vma->mm_vm, t_start, t_end, t_perm);
		if (!t_vma)
			goto err;
	}

	//TODO remove everything between vma and last and free them.
	//do vma->prev -> f_vma -> s_vma (-> t_vma) -> last.next
	//apply everything on pageroot.
	vma = vma->lk_areas.prev;
	last = last->lk_areas.next;
	mm_delete_region(mm, vma, last);
	Q_INSERT_AFTER(mm->mmap, vma, f_vma, lk_areas);
	Q_INSERT_AFTER(mm->mmap, f_vma, s_vma, lk_areas);
	if (t_vma)
		Q_INSERT_AFTER(mm->mmap, s_vma, t_vma, lk_areas);

	//TODO apply pgroot for f_vma, s_vma, and (t_vma)? t_vma.
	//TODO do this before.
	if (mm_apply_to_pgroot(f_vma))
		goto err;
	if (s_vma && mm_apply_to_pgroot(s_vma))
		goto err;
	if (t_vma && mm_apply_to_pgroot(t_vma))
		goto err;

err:
	if (f_vma)
		free(f_vma);
	if (s_vma)
		free(s_vma);
	if (t_vma)
		free(t_vma);
	return -ENOMEM;
}

int mm_apply_to_pgroot(vm_area_struct *vma)
{
	if (!vma || !(vma->vm_mm) || !(vma->vm_mm->pml4))
		return -EINVAL;
	
	//TODO call proper dune page_walk of dune_map_phys or whatev'.
	return 0;
}

// int mm_insert(mm_struct *mm, vm_area_struct *vma) {
// 	if (!(mm->mmap->head)) {
// 		Q_INSERT_TAIL(mm->mmap, vma);
// 		return 0;
// 	}
	
// 	l_vm_area *result = malloc(sizeof(l_vm_area));
// 	if (!result)
// 		return -ENOMEM;

// 	vm_area_struct *iter = mm->mmap->head, vm_area_struct *new_vma = vma;
// 	while(iter != NULL) {
// 		if (iter->vm_end < new_vma->vm_start) {
// 			vm_area_struct *tmp = iter;
// 			iter = iter->lk_areas.next;
// 			Q_INSERT_TAIL(result, tmp, lk_areas);
// 		} else if (iter->vm_start > new_vma->vm_end) {
// 			Q_INSERT_TAIL(result, new_vma, lk_areas);
// 			new_vma = iter;
// 		} else if ()
// 	}

}