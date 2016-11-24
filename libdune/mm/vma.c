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

	return vma;
}

//FIXME: should we implement the unmapping here instead?
int vma_free(vm_area_struct *vma)
{
	assert(vma);
	free(vma);
	return 0;
}

vm_area_struct *vma_copy(vm_area_struct *vma, bool cow)
{
	assert(vma);
	vm_area_struct *copy = malloc(sizeof(vm_area_struct));
	if (!copy) goto err;
	
	/* We do not cow something that is shared.*/
	if (cow) {
		vma->dirty = 1;
	}

	*copy = *vma;
	return copy;
err:
	return NULL;
}

vm_area_struct *vma_cow_copy(vm_area_struct *vma)
{
	return vma_copy(vma, true);
}

vm_area_struct *vma_shared_copy(vm_area_struct *vma)
{
	return vma_copy(vma, false);
}


void vma_dump(vm_area_struct *vma)
{
	assert(vma);
	printf("0x%016lx-0x%016lx,", vma->vm_start, vma->vm_end);
	printf(" flags: %016lx, ", vma->vm_flags);
	printf("dirty: %d, user: %d.\n", vma->dirty, vma_is_user(vma));
	fflush(stdout);
}






