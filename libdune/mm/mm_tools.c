#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <malloc.h>

#include "vm.h"
#include "vm_tools.h"
#include "mm.h"
#include "mm_tools.h"

#define DEBUG_MM

//FIXME: change the interface, it doesn't make sense to have both apply and cow.
mm_struct* mm_copy(mm_struct *mm, bool apply, bool cow)
{
	assert(mm && mm->mmap);
	vm_area_struct *current = NULL;

	//TODO: remove, for debugging.
	mm_verify_mappings(mm);
	

	mm_struct *copy = malloc(sizeof(mm_struct));
	if (!copy) goto err;

	copy->mmap = malloc(sizeof(l_vm_area));
	if (!(copy->mmap)) goto err;

	Q_INIT_ELEM(copy, lk_mms);
	Q_INIT_HEAD(copy->mmap);

	Q_FOREACH(current, mm->mmap, lk_areas) {
		vm_area_struct *vmcpy = NULL;
		/* The kernel mappings and read only pages are never cowed.*/
		if (!vma_is_user(current) || !(current->vm_flags & PERM_W)) {
			vmcpy = vma_copy(current, false);
		} else {
			vmcpy = vma_copy(current, cow);
		}
		
		if (vmcpy == NULL) goto err;
		
		vmcpy->vm_mm = copy;
		Q_INSERT_TAIL(copy->mmap, vmcpy, lk_areas);
	}

	/* Do we have to apply the changes?*/
	if (apply && cow) {
		mm_apply(mm);
	}
	copy->pml4 = vm_pgrot_copy(mm->pml4, cow);
	assert(copy->pml4 != NULL);

#ifdef DEBUG_MM
	mm_assert_equals(mm, copy);
	vm_compare_pgroots(mm->pml4, copy->pml4);
	//FIXME: bug is here.
	mm_verify_mappings(mm);
	mm_verify_mappings(copy);
#endif

	return copy;
err:
	if (copy)
		mm_free(copy);
	return NULL;
}

int mm_overlap(vm_area_struct *vma, vm_addrptr start, vm_addrptr end)
{
	int dont_overlap = (vma->vm_end) <= (start) || (vma->vm_start) >= (end);
	return !(dont_overlap);
}

int mm_split_or_merge(	mm_struct *mm,
						vm_area_struct *vma,
						vm_addrptr start,
						vm_addrptr end,
						unsigned long perm,
						mm_cb_ft f,
						void *args)
{
	assert(vma);
	assert(start <= end);
	int ret;
	vm_area_struct *iter = NULL, *last = NULL;
	
	fflush(stdout);
	/* Find the last conflicting block.*/
	for (iter = vma->lk_areas.next; iter != NULL; iter = iter->lk_areas.next) {
		if (!mm_overlap(iter, start, end)) {
			last = iter->lk_areas.prev;
			break;
		}
	}
	/* Check that last is correctly set.*/
	last = (last != NULL)? last : mm->mmap->last;
	assert(last != NULL);

	/*1. creates only one vma, it is a merge 
	 *2. creates only two blocks.
	 *	2.1. f_vma is the new block, s_vma is the rest.
	 *	2.2. f_vma is the old block, s_vma is the new block.
	 *3. creates three blocks.*/
	vm_area_struct *f_vma = NULL, *s_vma = NULL, *t_vma = NULL;

	/*1.*/
	if (start <= vma->vm_start && end >= last->vm_end) {
		f_vma = vma_create(vma->vm_mm, start, end, perm);
		if (!f_vma) {
			ret = -ENOMEM;
			goto err;
		}
		goto alloc;
	}

	/*2.*/
	vm_addrptr f_start, f_end, s_start, s_end, t_start, t_end;
	unsigned long f_perm, s_perm, t_perm;

	/*2.1*/
	if (vma->vm_start == start && end < last->vm_end) {
		f_start = start; f_end = end; f_perm = perm;

		s_start = end; s_end = last->vm_end; s_perm = last->vm_flags;
		goto create12;
	}

	/*2.2*/
	if (vma->vm_start < start && end == last->vm_end) {
		f_start = vma->vm_start; f_end = start; f_perm = vma->vm_flags;

		s_start = start; s_end = end; s_perm = perm;
		goto create12;
	}

	/*3.*/
	/*TODO: Safety check, remove after making sure it works.*/
	assert(start > vma->vm_start && end < last->vm_end);

	f_start = vma->vm_start; f_end = start; f_perm = vma->vm_flags;

	s_start = start; s_end = end; s_perm = perm;

	t_start = end; t_end = last->vm_end; t_perm = last->vm_flags;

	t_vma = vma_create(vma->vm_mm, t_start, t_end, t_perm);
	if (!t_vma) {
		ret = -ENOMEM;
		goto err;
	}

create12:
	f_vma = vma_create(vma->vm_mm, f_start, f_end, f_perm);
	if (!f_vma) {
		ret = -ENOMEM;
		goto err;
	}

	s_vma = vma_create(vma->vm_mm, s_start, s_end, s_perm);
	if (!s_vma) {
		ret = -ENOMEM;
		goto err;
	}

alloc: ;
	/* Identify which vma needs to get a remapping in the page tables.*/
	vm_area_struct *to_remap = f_vma;
	if (s_vma && !t_vma) {
		to_remap = (s_vma->vm_start == start)? s_vma : f_vma;
	}
	if (t_vma) {
		to_remap = s_vma;
	}

	assert(to_remap->vm_start == start);
	assert(to_remap->vm_end == end);
	assert(to_remap->vm_flags == perm);
	
	/* The mm_apply is only done on the portion that maps the given pa.*/
	if ((ret = f(to_remap, args)))
		goto err;
	
	/* Clean up the vmas.*/
	vm_area_struct *in_q = vma->lk_areas.prev;
	mm_delete_region(mm, vma->lk_areas.prev, last->lk_areas.next);
	
	/* Insert the f_vma*/
	if (in_q) {
		Q_INSERT_BEFORE(mm->mmap, in_q, f_vma, lk_areas);
	}
	else {
		Q_INSERT_FRONT(mm->mmap, f_vma, lk_areas);
	} 
		
	if (s_vma) {
		Q_INSERT_AFTER(mm->mmap, f_vma, s_vma, lk_areas);
	}
	if (t_vma) {
		Q_INSERT_AFTER(mm->mmap, s_vma, t_vma, lk_areas);
	}

	return 0;
err:
	if (f_vma)
		free(f_vma);
	if (s_vma)
		free(s_vma);
	if (t_vma)
		free(t_vma);
	return ret;
}

/* Removes the vmas between start (not included) and end (not included) for the mm->mmap.
 * Frees the removed vmas.*/
int mm_delete_region(	mm_struct *mm,
						vm_area_struct *start,
						vm_area_struct *end)
{
	vm_area_struct *iter = (!start)? mm->mmap->head : start->lk_areas.next;
	while (iter != NULL) {
		vm_area_struct *tmp = iter;
		iter = iter->lk_areas.next;

		Q_REMOVE(mm->mmap, tmp, lk_areas);
		vma_free(tmp);
		if (iter == end)
			break;
	}

	return 0;
}

int mm_free(mm_struct *mm)
{
	assert(mm);
	int ret = 0;
	if (!mm->mmap) {
		free(mm);
		return 0;
	}

	vm_area_struct *iter = mm->mmap->head;
	while(iter != NULL) {
		vm_area_struct *current = iter;
		iter = iter->lk_areas.next;
		vma_free(current);
	}

	free(mm->mmap);
	if (mm->pml4)
		dune_vm_free(mm->pml4);
	free(mm);
	return ret;
}

void mm_uncow(mm_struct *mm, vm_addrptr va)
{
	vm_area_struct *current = NULL;
	vm_area_struct *found = NULL;
	vm_addrptr addr = MM_PGALIGN_DN(va);

	//TODO: keep only the else. This is for debugging purposes.
	Q_FOREACH(current, mm->mmap, lk_areas) {
		if (current->vm_start == addr &&
			current->vm_end == (addr + PGSIZE)) {
			found = current;

			assert(current->vm_flags & PERM_W);
			assert(current->vm_flags & PERM_U);
			break;
		} else if (mm_overlap(current, addr, addr + PGSIZE)) {
			/* There is no allocation smaller than one page, and everything is 
			 * page-aligned, hence the fault should overlap with a single vma.*/
			assert(current->vm_start <= addr);
			assert(current->vm_end >= (addr + PGSIZE));
			found = current;
			break;
		}
	}
	assert(found != NULL);
	vm_uncow(mm->pml4, (void*) va);

	//TODO: check the mm now.
	mm_verify_mappings(mm);
}

/******************************************************************************/
/*					Debugging functions										  */
/******************************************************************************/

void mm_dump(mm_struct *mm)
{
	assert(mm);
	vm_area_struct *current = NULL;
	Q_FOREACH(current, mm->mmap, lk_areas) {
		vma_dump(current);
	}
}

static void __compare_permissions(unsigned long flags, ptent_t pte)
{
	if (flags & PERM_U)
		assert(pte & PTE_U);
	else 
		assert(!(pte & PTE_U));

	if (pte & PTE_W)
		assert(flags & PERM_W);
	else 
		assert(!(flags & PERM_W) || ((flags & PERM_U) && (pte & PTE_COW)));

	assert(!(flags & PERM_COW));

	if (pte & PTE_COW)
		assert((flags & PERM_W) && (flags & PERM_U));

	//TODO: check that this is correct.
	if (flags & PERM_X)
		assert(!(pte & PTE_NX));
	// else 
	// 	assert(pte & PTE_NX);

	if (pte_big(pte))
		assert(flags & PERM_BIG || flags & PERM_BIG_1GB);
	else 
		assert(!(flags & PERM_BIG || flags & PERM_BIG_1GB));
}

int mm_verify_mappings(mm_struct *mm)
{
	int ret = 0;
	assert(mm);
	assert(mm->mmap);
	assert(mm->pml4);
	vm_area_struct *current = NULL;

	Q_FOREACH(current, mm->mmap, lk_areas) {
		/* Check the start.*/
		ret = dune_vm_has_mapping(mm->pml4, (void*) current->vm_start);
		assert(ret == 0);

		/* Check the middle.*/
		vm_addrptr mid = (current->vm_start + current->vm_end) /2;
		ret = dune_vm_has_mapping(mm->pml4, (void*) mid);
		assert(ret == 0);

		/* Lookup the rights in page table and compare with vma.*/
		ptent_t *pte = NULL;
		ret = vm_lookup(mm->pml4, (void*) current->vm_start, &pte, CREATE_NONE, 0);
		assert(pte);
		__compare_permissions(current->vm_flags, *pte);

		/* Check that we do not use forbidden flags (i.e., PERM_COW)*/
		assert(!(current->vm_flags & PERM_COW));
	}
	return 0;
}

int mm_assert_equals(mm_struct *o, mm_struct *c)
{
	vm_area_struct *o_current = NULL, *c_current = NULL;
	for (o_current = o->mmap->head, c_current = c->mmap->head;
		o_current != NULL && c_current != NULL;
		o_current = o_current->lk_areas.next,
		c_current = c_current->lk_areas.next) {

		assert(o_current->vm_start == c_current->vm_start);
		assert(o_current->vm_end == c_current->vm_end);
		assert(o_current->vm_flags == c_current->vm_flags);
	}
	/* Assert they have the same size.*/
	assert(o_current == NULL && c_current == NULL);
	return 0;
}