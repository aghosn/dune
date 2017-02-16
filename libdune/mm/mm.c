#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>
#include <errno.h>
#include <sys/ioctl.h>

#include "../local.h"
#include "../dune.h"
#include "mm.h"
#include "mm_types.h"
#include "memory.h"
#include "vma.h"
#include "vm_tools.h"

/*Global variables for memory limits.*/
uintptr_t phys_limit;
uintptr_t mmap_base;
uintptr_t stack_base;
int dune_fd;

#define VSYSCALL_ADDR 0xffffffffff600000

static void __mm_setup_mappings_cb(const struct dune_procmap_entry *ent)
{
	int ret;
	mm_struct * current_mm = memory_get_mm();
	/* page region already mapped*/
	if (ent->begin == (unsigned long) PAGEBASE)
		return;

	if (ent->begin == (unsigned long) VSYSCALL_ADDR) {
		vm_addrptr start = VSYSCALL_ADDR;
		vm_addrptr end = VSYSCALL_ADDR + PGSIZE;
		void * pa =(void *)dune_va_to_pa(&__dune_vsyscall_page);
		
		unsigned long perm = PERM_W;
		ret = mm_create_phys_mapping(current_mm, start, end, pa, perm);
		
		ASSERT_DBG(ret == 0,
		"start{0x%016lx}, end{0x%016lx}, pa{0x%016lx}, perm{0x%016lx}.\n",
		start, end,(unsigned long)pa, perm);
		
		return;
	}

	vm_addrptr start = (vm_addrptr) ent->begin;
	vm_addrptr end = (vm_addrptr) ent->end;
	void *pa = (void *)dune_va_to_pa((void*) ent->begin);
	unsigned int perm = PERM_NONE;

	if (ent->type == PROCMAP_TYPE_VDSO) {
		/* VDSO is accessed in user mode for execution.*/
		perm |= PERM_U | PERM_R | PERM_X;
		ret = mm_create_phys_mapping(current_mm, start, end, pa, perm);
		
		ASSERT_DBG(ret == 0,
			"start{0x%016lx}, end{0x%016lx}, pa{0x%016lx}, perm{0x%016lx}\n",
			start, end, (unsigned long)pa, (unsigned long)perm);

		return;
	}

	if (ent->type == PROCMAP_TYPE_VVAR) {
		/* I leave the user permission since we do not have write access.*/
		perm |= PERM_U | PERM_R;
		ret = mm_create_phys_mapping(current_mm, start, end, pa, perm);
		
		ASSERT_DBG(ret == 0,
			"start{0x%016lx}, end{0x%016lx}, pa{0x%016lx}, perm{0x%016lx}\n",
			start, end, (unsigned long)pa, (unsigned long)perm);

		return;
	}

	if (ent->r)
		perm |= PERM_R;
	if (ent->w)
		perm |= PERM_W;
	if (ent->x)
		perm |= PERM_X;

	/* Other physical mappings. For them we do not add user permission.*/
	ret = mm_create_phys_mapping(current_mm, start, end, pa, perm);
	
	ASSERT_DBG(ret == 0,
			"start{0x%016lx}, end{0x%016lx}, pa{0x%016lx}, perm{0x%016lx}\n",
			start, end, (unsigned long)pa, (unsigned long)perm);
}

/* Init the memory regions.
 * Must be called by dune_memory_init, otherwise will fail.
 * TODO: change the permissions, not sure dune's ones are correct.*/
int mm_init()
{
	int ret;
	void *start = NULL, *end = NULL, *pa = NULL;
	unsigned long perm = 0;

	ASSERT_DBG(mm_root != NULL, "mm_root is null.\n");
	
	mm_struct *current_mm = memory_get_mm();
	
	ASSERT_DBG(current_mm == mm_root,
		"current_mm{%p}, mm_root{%p}\n", current_mm, mm_root);

	/* Map the page base. */
	start = (void*) PAGEBASE;
	end = (void*) (PAGEBASE + MAX_PAGES * PGSIZE);
	pa = (void*) dune_va_to_pa((void*)PAGEBASE);
	

	perm = PERM_R | PERM_W | PERM_BIG;

	 if ((ret = mm_create_phys_mapping(current_mm,(vm_addrptr) start,
	 	(vm_addrptr) end, pa, perm)))
	 	return ret;

	/*Map the procmap.*/
	dune_procmap_iterate(&__mm_setup_mappings_cb);

	//TODO: for debugging, remove afterwards.
	//mm_verify_mappings(current_mm);
	
	return 0;
}

vm_area_struct* mm_find(mm_struct *mm, vm_addrptr addr, bool is_end)
{
	ASSERT_DBG(mm, "mm is null.\n");
	vm_area_struct *res = NULL, *current = NULL;

	/* Fast paths.*/

	/* Before the start.*/
	if (mm->mmap->head->vm_start >= addr) {
		if (!is_end)
			res = mm->mmap->head;
		goto ret;
	}

	/* After the end.*/
	if (mm->mmap->last->vm_end <= addr) {
		if (is_end)
			res = mm->mmap->last;
		goto ret;
	}

	/* Slow path.*/
	Q_FOREACH(current, mm->mmap, lk_areas) {
		/* Within the vma.*/
		if (current->vm_start <= addr && current->vm_end > addr) {
			res = current;
			break;
		}

		/* Address is between the two vmas.*/
		if (res && res->vm_end < addr && current->vm_start > addr) {
			if (!is_end)
				res = current;
			break;
		}

		res = current;
	}
ret:
	ASSERT_DBG(res, "Could not find a result.\n");
	return res;
}

int mm_create_phys_mapping(mm_struct *mm, 
							vm_addrptr va_start, 
							vm_addrptr va_end, 
							void* pa, 
							unsigned long perm)
{
	ASSERT_DBG(mm->mmap, "mm->mmap is null.\n"); 
	ASSERT_DBG(va_start < va_end,
		"va_start{0x%016lx}, va_end{0x%016lx}.\n", va_start, va_end);
	
	int ret = 0;
	vm_area_struct *current = NULL;

	/* Make sure the addresses are page aligned*/
	va_start = MM_PGALIGN_DN(va_start);
	va_end = MM_PGALIGN_UP(va_end);
	
	if(!(mm->mmap))
		return -EINVAL;

	/* Fast paths*/

	/* Insert at the front.*/
	if (mm->mmap->head == NULL || mm->mmap->head->vm_start >= va_end) {
		vm_area_struct *vma = vma_create(mm, va_start, va_end, perm);
		if (!vma)
			return -ENOMEM;
		Q_INSERT_FRONT(mm->mmap, vma, lk_areas);
		ret = mm_apply_to_pgroot(vma, pa);
		return ret;
	}
	
	/* Insert at the end.*/
	if (mm->mmap->last->vm_end <= va_start) {
		vm_area_struct *vma = vma_create(mm, va_start, va_end, perm);
		if (!vma)
			return -ENOMEM;
		Q_INSERT_TAIL(mm->mmap, vma, lk_areas);
		ret = mm_apply_to_pgroot(vma, pa);
		return ret;
	}
	
	/* Slow path*/
	Q_FOREACH(current, mm->mmap, lk_areas) {
		if (current->vm_start == va_start &&
			current->vm_end == va_end &&
			current->vm_flags == perm) {
			/* The mapping already exists*/
			//TODO: need to redo the mapping if that maps a new physical address.
			return 0;
		}

		if (mm_overlap(current, va_start, va_end)) {
			ret = mm_split_and_merge(mm, current, va_start, va_end, perm, 
				&mm_apply_to_pgroot, pa);

			return ret;
		}

		/*Try to insert*/
		if (current->vm_start >=  va_end) {
			vm_area_struct *vma = vma_create(mm, va_start, va_end, perm);
			if (!vma)
				return -ENOMEM;
			Q_INSERT_BEFORE(mm->mmap, current, vma, lk_areas);
			ret = mm_apply_to_pgroot(vma, pa);
			return ret;
		}
	}
	/*Should never come here.*/
	ASSERT_DBG(0, "Should not have reached this.\n");
	return ret;
}

int mm_apply_to_pgroot(vm_area_struct *vma, void *pa)
{
	ASSERT_DBG(vma && vma->vm_mm && vma->vm_mm->pml4, "uninitialized element.\n");

	if (pa != NULL) {
		return dune_vm_map_phys(vma->vm_mm->pml4, (void*)vma->vm_start,
		(size_t)(vma->vm_end - vma->vm_start), pa, vma->vm_flags);
	}

	//FIXME: function is actually never called without a physical address
	ASSERT_DBG(0, "Should not have reached this part.\n");
	return 1;
}

static int __mm_apply_protect(vm_area_struct *vma, void* perm)
{
	ASSERT_DBG(perm != NULL, "The permissions are null.\n");
	ASSERT_DBG(vma && vma->vm_mm && vma->vm_mm->pml4, "Null params.\n");
	ASSERT_DBG(vma->vm_flags == *((unsigned long*)perm),
		"vm_flags{0x%016lx}, perm{0x%016lx}.\n",
		vma->vm_flags, *((unsigned long*)perm));

	return dune_vm_mprotect(vma->vm_mm->pml4,(void*)(vma->vm_start),
		(size_t)(vma->vm_end - vma->vm_start), vma->vm_flags);
}

/* Modifies the permissions for the vmas that map the provided range.
 * If the virtual memory region is not mapped, it is NOT created.
 * If the start or end address is within a vma, we split it (if possible).*/
int mm_mprotect(mm_struct *mm, vm_addrptr start,
				vm_addrptr end, unsigned long perm)
{
	ASSERT_DBG(mm, "mm is null.\n");
	int ret = -1;
	vm_area_struct *current = NULL;
	/* Page align the start and end.*/
	start = MM_PGALIGN_DN(start);
	end = MM_PGALIGN_UP(end);

	Q_FOREACH(current, mm->mmap, lk_areas) {
		if (current->vm_start == start &&
			current->vm_end == end &&
			current->vm_flags == perm) {
			/* The mapping already exists with the proper flags*/
			return 0;
		}

		if (mm_overlap(current, start, end)) {
			ret = mm_split_no_merge(mm, current, start, end, perm, 
				&__mm_apply_protect, &perm);
			return ret;
		}
	}
	
	/* Should never come here.*/
	ASSERT_DBG(0, "Should not have reached this part.\n"); 
	return -EINVAL;
}

static int __mm_unmap(vm_area_struct *vma, void *args)
{
	ASSERT_DBG(vma != NULL, "vma is null.\n");
	vm_area_struct **pt = ((vm_area_struct**)args);
	*pt = vma;
	return 0;
}

int mm_unmap(mm_struct *mm, vm_addrptr start, vm_addrptr end, bool apply)
{
	ASSERT_DBG(start < end, "start{0x%016lx}, end{0x%016lx}\n", start, end);
	int ret = 0;
	vm_area_struct *current = NULL, *to_rm = NULL;
	
	/* Align the addresses.*/
	start = MM_PGALIGN_DN(start);
	end = MM_PGALIGN_UP(end);

	Q_FOREACH(current, mm->mmap, lk_areas) {
		if (mm_overlap(current, start, end)) {
			ret = mm_split_and_merge(mm, current, start, end, 0, 
				&__mm_unmap, &to_rm);
			
			if (ret)
				return ret;
			
			assert(to_rm != NULL);
			Q_REMOVE(mm->mmap, to_rm, lk_areas);
		
			if (apply) {
				/* Cow pages references are handled at page table level.*/
				dune_vm_unmap(mm->pml4, (void*)(to_rm->vm_start), 
					(size_t)(to_rm->vm_end - to_rm->vm_start));
			}
			vma_free(to_rm);
			
			return ret;
		}
	}
	/* Need to decide if we return 0 or it is a fault to unmap invalid addr.*/
	ASSERT_DBG(0, "Should not have reached this part.\n");
	return -EINVAL;
}

void mm_apply(mm_struct *mm)
{
	vm_area_struct *current = NULL;
	Q_FOREACH(current, mm->mmap, lk_areas) {
		
		ASSERT_DBG(current->vm_mm == mm,
			"current->vm_mm{%p}, mm{%p}.\n", current->vm_mm, mm);
		
		if (!vma_is_user(current))
			continue;
		if (current->dirty) {
			size_t len = (size_t)(current->vm_end - current->vm_start);
			dune_vm_mprotect(mm->pml4,(void*)(current->vm_start), len, 
				current->vm_flags);
			current->dirty = 0;
		}
	}
}