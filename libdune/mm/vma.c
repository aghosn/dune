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

vm_area_struct *vma_cow_copy(vm_area_struct *vma)
{
	assert(vma);
	vm_area_struct *copy = malloc(sizeof(vm_area_struct));
	if (!copy)
		goto err;

	l_vm_area *shared = vma->head_shared;
	assert(!vma->shared);
	
	if (!vma->cow) {
		assert(shared == NULL);
		shared = malloc(sizeof(l_vm_area));
		if (!shared) goto err;

		Q_INIT_HEAD(shared);
		Q_INIT_ELEM(vma, lk_shared);
		Q_INSERT_TAIL(shared, vma, lk_shared);

		vma->head_shared = shared;
		vma->cow = 1;
		vma->vm_flags ^= PERM_W;
		vma->vm_flags |= PERM_COW;
		vma->dirty = 1;
	}

	memcpy(copy, vma, sizeof(vm_area_struct));
	Q_INIT_ELEM(copy, lk_shared);
	Q_INSERT_TAIL(shared, copy, lk_shared);
	copy->dirty = 1;

	return copy;
err:
	if (copy)
		free(copy);
	return NULL;
}

vm_area_struct *vma_shared_copy(vm_area_struct *vma)
{
	assert(vma);
	vm_area_struct *copy = malloc(sizeof(vm_area_struct));
	if (!copy)
		goto err;

	l_vm_area *shared = vma->head_shared;

	if (!vma->shared || vma->head_shared == NULL) {
		assert(shared == NULL);
		assert(vma->cow == 0);
		
		shared = malloc(sizeof(l_vm_area));
		if (!shared) goto err;

		Q_INIT_HEAD(shared);
		Q_INIT_ELEM(vma, lk_shared);
		Q_INSERT_TAIL(shared, vma, lk_shared);

		vma->head_shared = shared;
		vma->shared = 1;
		vma->dirty = 1;
	}
	
	memcpy(copy, vma, sizeof(vm_area_struct));
	Q_INIT_ELEM(copy, lk_shared);
	Q_INSERT_TAIL(shared, copy, lk_shared);
	copy->dirty = 1;
	return copy;
err:
	if (copy)
		free(copy);
	return NULL;
}