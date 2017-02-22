#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include "vma.h"

vm_area_struct *vma_create(	mm_struct *mm,
							vm_addrptr start,
							vm_addrptr end,
							unsigned long perm)
{
	ASSERT_DBG(start == MM_PGALIGN_DN(start), "start{0x%016lx}.\n", start);
	ASSERT_DBG(end == MM_PGALIGN_UP(end), "end{0x%016lx}.\n", end);

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
	ASSERT_DBG(vma, "vma is null.\n");
	//printf("Vma address %p and %p\n", vma, &vma->vm_flags);
	//TODO aghosn: triggers a bug if I try to free it ...
	//free(vma);
	return 0;
}

vm_area_struct *vma_copy(vm_area_struct *vma, bool cow)
{
	ASSERT_DBG(vma, "vma is null.\n");
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
	ASSERT_DBG(vma, "vma is null.\n");
	printf("0x%016lx-0x%016lx,", vma->vm_start, vma->vm_end);
	printf(" flags: %016lx, ", vma->vm_flags);
	printf("dirty: %d, user: %d.\n", vma->dirty, vma_is_user(vma));
	fflush(stdout);
}






