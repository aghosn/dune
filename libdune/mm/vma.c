#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <assert.h>

#include "vma.h"

vm_area_struct *vma_create(	mm_struct *mm,
							vm_addrptr start,
							vm_addrptr end,
							unsigned long perm)
{
	assert(start == MM_PGALIGN_DN(start));
	assert(end == MM_PGALIGN_UP(end));

	vm_area_struct *vma = malloc(sizeof(vm_area_struct));
	if (!vma)
		return NULL;
	//TODO: check if not faster to set extra flags by hand.
	memset(vma, 0, sizeof(vm_area_struct));
	vma->vm_start = start;
	vma->vm_end = end;
	vma->vm_flags = perm;
	vma->vm_mm = mm;

	if (perm & PERM_U)
		vma->user = 1;
	else {
		/* We do not allocate the shared list yet.*/
		vma->shared = 1;
	}

	return vma;
}

int vma_free(vm_area_struct *vma)
{
	assert(vma);
	if ((vma->cow || vma->shared) && vma->head_shared) {
		l_vm_area *sh = vma->head_shared;
		assert(sh);
		Q_REMOVE(sh, vma, lk_shared);
		
		/* it was the last one here.*/
		if (sh->head == NULL) {
			assert(sh->last == NULL);
			free(sh);
		}
	}
	free(vma);
	return 0;
}

vm_area_struct *vma_copy(vm_area_struct *vma, bool cow)
{
	assert(vma);
	vm_area_struct *copy = malloc(sizeof(vm_area_struct));
	if (!copy) goto err;

	l_vm_area *shared = vma->head_shared;
	
	/* We do not cow something that is shared.*/
	if (cow) assert(!vma->shared);

	/* This vma was never shared nor cowed.*/
	if (!shared) {
		assert(vma->cow == 0);
		shared = malloc(sizeof(l_vm_area));
		if (!shared) goto err;

		Q_INIT_HEAD(shared);
		Q_INIT_ELEM(vma, lk_shared);
		Q_INSERT_TAIL(shared, vma, lk_shared);
		vma->head_shared = shared;
		if (!cow)
			vma->shared = 1;
	} else {
		assert(vma->shared || vma->cow);
	}

	/* This copy is a cow, so we need to change the flags.*/
	if (cow && (vma->cow == 0)) {
		assert(!(vma->vm_flags & PERM_COW));
		vma->cow = 1;
		vma->dirty = 1;
		vma->vm_flags &= ~(PERM_W);
		vma->vm_flags |= PERM_COW;
	}

	assert(shared != NULL);
	/* Copy the vma content.*/
	*copy = *vma;
	Q_INIT_ELEM(copy, lk_shared);
	Q_INIT_ELEM(copy, lk_areas);
	Q_INSERT_TAIL(shared, copy, lk_shared);
	copy->dirty;

	return copy;
err:
	//FIXME:
	return NULL;
}

vm_area_struct *vma_cow_copy(vm_area_struct *vma)
{
	vma_copy(vma, true);
}

vm_area_struct *vma_shared_copy(vm_area_struct *vma)
{
	vma_copy(vma, false);
}


void vma_dump(vm_area_struct *vma)
{
	assert(vma);
	printf("0x%016lx-0x%016lx, flags: %016lx, user: %d, cow: %d, shared: %d\n",
	vma->vm_start, vma->vm_end, vma->vm_flags, vma->user, vma->cow, vma->shared);
}






