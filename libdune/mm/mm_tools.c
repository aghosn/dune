#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <malloc.h>

#include "mm.h"
#include "mm_tools.h"


struct __shared_info {
	l_vm_area *areas;
	bool apply;
};

static int __mm_cow(vm_area_struct *vma, void *args)
{
	struct __shared_info *sh = (struct __shared_info*) args;
	vma->dirty = 1;
	vma->cow = 1;
	vma->vm_flags |= PERM_COW;
	vma->shared = 0;

	if (sh->apply) {
		mm_apply_to_pgroot(vma, NULL);
		vma->dirty = 0;
	}
	if (!(sh->areas)) {
		//TODO: what if malloc fails?
		sh->areas = malloc(sizeof(l_vm_area));
		Q_INIT_HEAD(sh->areas);
	}

	vma->head_shared = sh->areas;
	Q_INSERT_TAIL(sh->areas, vma, lk_shared);
	return 0;
}

int mm_cow(mm_struct *o, mm_struct *c, vm_addrptr s, vm_addrptr e, bool apply)
{
	//TODO: implement copy on write.
	assert(s < e);
	int ret = 0;
	vm_area_struct *current = NULL;
	struct __shared_info shared = {NULL, true};
	Q_FOREACH(current, o->mmap, lk_areas) {
		if (mm_overlap(current, s, e)) {
			//TODO: add the COW flag
			//FIXME: problem if already cow within it.
			ret = mm_split_or_merge(o, current, s, e, current->vm_flags,
				&__mm_cow, &shared);
			break;
		}
	}
	if (ret)
		return ret;
	current = NULL;
	shared.apply = apply;
	Q_FOREACH(current, c->mmap, lk_areas) {
		if (mm_overlap(current, s, e)) {
			ret = mm_split_or_merge(c, current, s, e, current->vm_flags,
				&__mm_cow, &shared);
			break;
		}
	}
	return ret;
}

static int __mm_shared(vm_area_struct *vma, void* args)
{
	struct __shared_info *sh = (struct __shared_info*) args;
	vma->dirty = 1;
	vma->cow = 0;
	vma->shared = 1;

	if (sh->apply) {
		mm_apply_to_pgroot(vma, NULL);
		vma->dirty = 0;
	}
	if (!(sh->areas)) {
		//TODO: what if malloc fails?
		sh->areas = malloc(sizeof(l_vm_area));
		Q_INIT_HEAD(sh->areas);
	}

	vma->head_shared = sh->areas;
	Q_INSERT_TAIL(sh->areas, vma, lk_shared);
	return 0;
}

int mm_shared(mm_struct *o, mm_struct *c, vm_addrptr s, vm_addrptr e, bool apply)
{
	//TODO: implement shared area.
	assert(s < e);
	int ret = 0;
	vm_area_struct *current = NULL;
	struct __shared_info shared = {NULL, true};
	//FIXME: can be optimized and go through both lists at the same time.
	Q_FOREACH(current, o->mmap, lk_areas) {
		if (mm_overlap(current, s, e)) {
			//TODO: Problem if region already shared...
			//TODO: how can we find the head?
			ret = mm_split_or_merge(o, current, s, e, current->vm_flags, 
				&__mm_shared, &shared);
			break;
		}
	}
	//FIXME: might need a cleanup?
	if (ret)
		return ret;
	current = NULL;
	shared.apply = apply;
	Q_FOREACH(current, c->mmap, lk_areas) {
		if (mm_overlap(current, s, e)) {
			ret = mm_split_or_merge(c, current, s, e, current->vm_flags,
				&__mm_shared, &shared);
			break;
		}
	}
	return ret;
}

int mm_into_root(mm_struct *mm)
{
	vm_area_struct *current = NULL;
	Q_FOREACH(current, mm->mmap, lk_areas) {
		vm_make_root((void*)(current->vm_start),(void*)(current->vm_end),
			mm->pml4);
		current->user = 0;
		current->vm_flags &= ~(PERM_U);
	}
	return 0;
}

int mm_count_entries(mm_struct *mm)
{
	vm_area_struct *current = NULL;
	vm_count_entries((void*)(mm->mmap->head->vm_start), (void*)(mm->mmap->last->vm_end),
			mm->pml4);
	
	return 0;
}