#include <stdio.h>
#include <stdlib.h>
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
	ASSERT_DBG(mm, "mm is null.\n");
	vm_area_struct *current = NULL;

	//TODO: remove, for debugging.
	mm_verify_mappings(mm);

	mm_struct *copy = malloc(sizeof(mm_struct));
	if (!copy) goto err;

	TAILQ_INIT(&(copy->mmap));

	TAILQ_FOREACH(current, &(mm->mmap), q_areas) {
		vm_area_struct *vmcpy = NULL;
		/* The kernel mappings and read only pages are never cowed.*/
		if (!vma_is_user(current) || !(current->vm_flags & PERM_W)) {
			vmcpy = vma_copy(current, false);
		} else {
			vmcpy = vma_copy(current, cow);
		}
		
		if (vmcpy == NULL) goto err;
		
		vmcpy->vm_mm = copy;
		TAILQ_INSERT_TAIL(&(copy->mmap), vmcpy, q_areas);
	}

	/* Do we have to apply the changes?*/
	if (apply && cow) {
		mm_apply(mm);
	}

	copy->pml4 = vm_pgrot_copy(mm->pml4, cow);
	ASSERT_DBG(copy->pml4, "copy->pml4 is null.\n");

#ifdef DEBUG_MM
	mm_assert_equals(mm, copy);
	vm_compare_pgroots(mm->pml4, copy->pml4);
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

int mm_split_and_merge(	mm_struct *mm,
						vm_area_struct *vma,
						vm_addrptr start,
						vm_addrptr end,
						unsigned long perm,
						mm_cb_ft f,
						void *args)
{
	ASSERT_DBG(vma, "vma is null.\n");
	ASSERT_DBG(start <= end, "start{0x%016lx}, end{0x%016lx}.\n", start, end);
	int ret;
	vm_area_struct *iter = NULL, *last = NULL;
	
	/* Find the last conflicting block.*/
	for (	iter = TAILQ_NEXT(vma, q_areas);
			iter != NULL;
			iter = TAILQ_NEXT(iter, q_areas)) {
		
		if (!mm_overlap(iter, start, end)) {
			last = TAILQ_PREV(iter, vm_area_list, q_areas);
			break;
		}
	}
	/* Check that last is correctly set.*/
	last = (last != NULL)? last : TAILQ_LAST(&(mm->mmap), vm_area_list);
	ASSERT_DBG(last, "last is null.\n");

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
	ASSERT_DBG(start > vma->vm_start && end < last->vm_end, 
	"start{0x%016lx}, vm_start{0x%016lx}, end{0x%016lx}, vm_end{0x%016lx}.\n",
	start, vma->vm_start, end , last->vm_end);

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

	
	ASSERT_DBG(	to_remap->vm_start == start,
				"to_remap->vm_start{0x%016lx}, start{0x%016lx}",
				to_remap->vm_start, start);

	ASSERT_DBG(	to_remap->vm_end == end,
				"to_remap->vm_end{0x%016lx}, end{0x%016lx}",
				to_remap->vm_end, end);
	
	ASSERT_DBG(	to_remap->vm_flags == perm,
				"to_remap->vm_flags{0x%016lx}, perm{0x%016lx}",
				to_remap->vm_flags, perm);
	
	/* The mm_apply is only done on the portion that maps the given pa.*/
	if ((ret = f(to_remap, args)))
		goto err;
	
	/* Clean up the vmas.*/
	vm_area_struct *in_q = TAILQ_PREV(vma, vm_area_list, q_areas);
	mm_delete_region(mm, TAILQ_PREV(vma, vm_area_list, q_areas),
		TAILQ_NEXT(last, q_areas));
	
	/* Insert the f_vma*/
	if (in_q) {
		TAILQ_INSERT_AFTER(&(mm->mmap), in_q, f_vma, q_areas);
	}
	else {
		TAILQ_INSERT_HEAD(&(mm->mmap), f_vma, q_areas);
	} 
		
	if (s_vma) {
		TAILQ_INSERT_AFTER(&(mm->mmap), f_vma, s_vma, q_areas);
	}
	if (t_vma) {
		TAILQ_INSERT_AFTER(&(mm->mmap), s_vma, t_vma, q_areas);
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

typedef struct snm_struct {
	vm_area_struct *n_head;
	mm_cb_ft f;
	void *args;
} snm_struct;

static int __mm_split_no_merge(vm_area_struct *vma, void* args)
{
	ASSERT_DBG(args, "args are null.\n");
	snm_struct *snm = (snm_struct*)(args);
	snm->n_head = vma;

	/* Do the proper callback.*/
	return (snm->f(vma, snm->args));
}


int mm_split_no_merge(	mm_struct *mm,
						vm_area_struct *vma,
						vm_addrptr start,
						vm_addrptr end,
						unsigned long perm,
						mm_cb_ft f,
						void *args)
{
	ASSERT_DBG(mm, "mm is null.\n");
	ASSERT_DBG(vma, "vma is null.\n");
	ASSERT_DBG(start <= end, "start{0x%016lx}, end{0x%016lx}", start, end);
	
	int ret;
	vm_area_struct *iter = NULL, *last = NULL;

	/* Find last conflicting block.*/
	for (	iter = TAILQ_NEXT(vma, q_areas);
			iter != NULL;
			iter = TAILQ_NEXT(iter, q_areas)) {

		if (!mm_overlap(iter, start, end)) {
			last = TAILQ_PREV(iter, vm_area_list, q_areas);
			break;
		}
	}

	/* Check that the last is correctly set.*/
	last = (last != NULL)? last : TAILQ_LAST(&(mm->mmap), vm_area_list);
	ASSERT_DBG(last != NULL, "last is null.\n");

	/* Fix the conflict with the last vma.*/
	snm_struct snm_last = {NULL, f, args};
	if (vma != last) {
		ret = mm_split_and_merge(mm, last, last->vm_start, end, perm, 
											&__mm_split_no_merge, &snm_last);
		if (ret)
			goto err;

		ASSERT_DBG(snm_last.n_head, "head is null.\n");
	}

	/* Fix the conflict with the first vma.*/
	vm_addrptr n_end = (vma->vm_end < end)? vma->vm_end : end;
	
	/* Get the pointer to the new head. After this call, vma might not be valid.*/
	snm_struct snm_first = {NULL, f, args};
	ret = mm_split_and_merge(mm, vma, start, n_end, perm, &__mm_split_no_merge,
																	&snm_first);
	if (ret)
		goto err;

	/* Should have a pointer to the new head.*/
	ASSERT_DBG(snm_first.n_head, "head is null.\n");

	/* Work is done, only one vm was conflicting.*/
	if (snm_last.n_head == NULL)
		return 0;

	/* Have vmas to modify in-between.*/
	for (iter = TAILQ_NEXT(snm_first.n_head, q_areas); 
		iter != NULL; 
		iter = TAILQ_NEXT(iter, q_areas)) {
		iter->vm_flags = perm;
		iter->dirty = 1;
		if ((ret = f(iter, args)))
			goto err;

		/* Reached the last vma.*/
		if (iter == snm_last.n_head)
			break;
	}
	
	return 0;
err:
	return ret;
}

/* Removes the vmas between start (not included) and end (not included) for the mm->mmap.
 * Frees the removed vmas.*/
int mm_delete_region(	mm_struct *mm,
						vm_area_struct *start,
						vm_area_struct *end)
{
	vm_area_struct *iter = (!start)? TAILQ_FIRST(&(mm->mmap)) : TAILQ_NEXT(start, q_areas);
	while (iter != NULL) {
		vm_area_struct *tmp = iter;
		iter = TAILQ_NEXT(iter, q_areas);

		TAILQ_REMOVE(&(mm->mmap), tmp, q_areas);
		vma_free(tmp);
		if (iter == end)
			break;
	}

	return 0;
}

int mm_free(mm_struct *mm)
{
	ASSERT_DBG(mm, "mm is null.\n");
	int ret = 0;

	vm_area_struct *iter = TAILQ_FIRST(&(mm->mmap));
	while(iter != NULL) {
		vm_area_struct *current = iter;
		iter = TAILQ_NEXT(iter, q_areas);
		vma_free(current);
	}

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

	TAILQ_FOREACH(current, &(mm->mmap), q_areas) {
		if (current->vm_start == addr &&
			current->vm_end == (addr + PGSIZE)) {
			found = current;
		
			ASSERT_DBG(current->vm_flags & PERM_W, "Missing write perm.\n");
			ASSERT_DBG(current->vm_flags & PERM_U, "Missing user perm.\n");
			break;
		} else if (mm_overlap(current, addr, addr + PGSIZE)) {
			/* There is no allocation smaller than one page, and everything is 
			 * page-aligned, hence the fault should overlap with a single vma.*/
			ASSERT_DBG(	current->vm_start <= addr,
						"current->vm_start{0x%016lx}, addr{0x%016lx}\n",
						current->vm_start, addr);

			ASSERT_DBG(current->vm_end >= (addr + PGSIZE),
				"current->vm_end{0x%016lx}, end{0x%016lx}\n",
				current->vm_end, (addr + PGSIZE));

			found = current;
			break;
		}
	}

	ASSERT_DBG(found != NULL, "Could not find the mapping to uncow %p.\n", (void*) va);

	vm_uncow(mm->pml4, (void*) va);

	mm_verify_mappings(mm);
}

int mm_verify_range(mm_struct *mm, vm_addrptr addr, uint64_t len)
{
	vm_area_struct *current = NULL;
	TAILQ_FOREACH(current, &(mm->mmap), q_areas) {
		if (mm_overlap(current, addr, addr + len)) {

			vm_area_struct *previous = current;
			vm_area_struct *runner = TAILQ_NEXT(previous, q_areas);
			while (runner != NULL && mm_overlap(runner, addr, addr + len)) {
				if (previous->vm_end < runner->vm_start)
					return 0;

				previous = runner;
				runner = TAILQ_NEXT(runner, q_areas);
			}
			ASSERT_DBG(previous != NULL, "mapping not found.");
			ASSERT_DBG(mm_overlap(previous, addr, addr + len), "error.");
			return (previous->vm_end > addr + len);
		}
	}
	return 0;
}
/******************************************************************************/
/*					Debugging functions										  */
/******************************************************************************/

void mm_dump(mm_struct *mm)
{
	ASSERT_DBG(mm, "mm is null.\n");
	vm_area_struct *current = NULL;
	TAILQ_FOREACH(current, &(mm->mmap), q_areas) {
		vma_dump(current);
	}
}

static void __compare_permissions(unsigned long flags, ptent_t pte)
{
	if (flags & PERM_U)
		ASSERT_DBG(pte & PTE_U, "missing user permission in pte.\n");
	else
		ASSERT_DBG(!(pte & PTE_U), "user perm in pte.\n");

	if (pte & PTE_W)
		ASSERT_DBG(flags & PERM_W, "missing write permission in flags.\n");
	else {
		ASSERT_DBG(!(flags & PERM_W) || ((flags & PERM_U) && (pte & PTE_COW)),
			"flags have too many permissions.\n");
	}

	ASSERT_DBG(!(flags & PERM_COW), "COW bits in flags.\n");

	if (pte & PTE_COW) {
		ASSERT_DBG((flags & PERM_W) && (flags & PERM_U),
					"Missing perms for COW in the flags.\n");
	}

	//TODO: check that this is correct.
	if (flags & PERM_X)
		ASSERT_DBG(!(pte & PTE_NX), "missing execute in pte.\n");
	// else 
	// 	assert(pte & PTE_NX);

	if (pte_big(pte)) {
		ASSERT_DBG(	flags & PERM_BIG || flags & PERM_BIG_1GB,
					"big not set in flags.\n");
	} else {
		ASSERT_DBG(!(flags & PERM_BIG || flags & PERM_BIG_1GB),
					"big not set in perm.\n"); 
	}
}

int mm_verify_mappings(mm_struct *mm)
{
	int ret = 0;
	ASSERT_DBG(mm, "mm is null.\n");
	ASSERT_DBG(mm->pml4, "mm->pml4 is null.\n");
	vm_area_struct *current = NULL, *prev = NULL;

	TAILQ_FOREACH(current, &(mm->mmap), q_areas) {
		
		/* Check that it's sorted.*/
		if (prev) {
			ASSERT_DBG(	prev->vm_end <= current->vm_start,
						"prev->vm_end{0x%016lx}, current->vm_start{0x%016lx}\n",
						prev->vm_end, current->vm_start);
		}

		/* Check the start.*/
		ret = dune_vm_has_mapping(mm->pml4, (void*) current->vm_start);
		ASSERT_DBG(ret == 0, "current->vm_start{0x%016lx}\n", current->vm_start);

		/* Check the middle.*/
		vm_addrptr mid = (current->vm_start + current->vm_end) /2;
		ret = dune_vm_has_mapping(mm->pml4, (void*) mid);
		ASSERT_DBG(ret == 0, "mid{0x%16lx}\n", mid);

		/* Lookup the rights in page table and compare with vma.*/
		ptent_t *pte = NULL;
		ret = vm_lookup(mm->pml4, (void*) current->vm_start, &pte, CREATE_NONE, 0);
		ASSERT_DBG(pte, "pte is null.\n");
		__compare_permissions(current->vm_flags, *pte);

		/* Check that we do not use forbidden flags (i.e., PERM_COW)*/
		ASSERT_DBG(!(current->vm_flags & PERM_COW), "using COW flags.\n");

		prev = current;
	}
	return 0;
}

int mm_assert_equals(mm_struct *o, mm_struct *c)
{
	vm_area_struct *o_current = NULL, *c_current = NULL;
	for (o_current = TAILQ_FIRST(&(o->mmap)),
		c_current = TAILQ_FIRST(&(c->mmap));
		o_current != NULL && c_current != NULL;
		o_current = TAILQ_NEXT(o_current, q_areas),
		c_current = TAILQ_NEXT(c_current, q_areas)) {

		ASSERT_DBG(o_current->vm_start == c_current->vm_start, 
			"o_current->vm_start{0x%016lx}, c_current->vm_start{0x%016lx}\n",
			o_current->vm_start, c_current->vm_start);

		ASSERT_DBG(o_current->vm_end == c_current->vm_end,
				"o_current->vm_end{0x%016lx}, c_current->vm_end{0x%016lx}\n",
				o_current->vm_end, c_current->vm_end);

		ASSERT_DBG(o_current->vm_flags == c_current->vm_flags,
			"o_current->vm_flags{0x%016lx}, c_current->vm_flags{0x%016lx}\n",
			o_current->vm_flags, c_current->vm_flags);
	}
	/* Assert they have the same size.*/
	ASSERT_DBG(o_current == NULL && c_current == NULL,
		"o_current{%p}, c_current{%p}\n", o_current, c_current);
	return 0;
}