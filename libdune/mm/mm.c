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
 * WARNING: does not affect the page tables. Should it?*/
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

#define VSYSCALL_ADDR 0xffffffffff600000
static void __mm_setup_vsyscall(void)
{
	ptent_t *pte;
	dune_vm_lookup(pgroot, (void *)VSYSCALL_ADDR, 1, &pte);
	*pte = PTE_ADDR(dune_va_to_pa(&__dune_vsyscall_page) | PTE_P | PTE_U);
}

static void __mm_setup_mappings_cb(const struct dune_procmap_entry *ent)
{
	int ret;
	/* page region already mapped*/
	if (ent->begin == (unsigned long) PAGEBASE)
		return;

	if (ent->begin == (unsigned long) VSYSCALL_ADDR) {
		vm_addrptr start = VSYSCALL_ADDR;
		vm_addrptr end = VSYSCALL_ADDR + PGSIZE;
		void * pa =(void *)dune_va_to_pa(&__dune_vsyscall_page);
		unsigned long perm = PTE_P | PTE_U;
		ret = mm_create_phys_mapping(mm_root, start, end, pa, perm);
		//__mm_setup_vsyscall(); //TODO check if lookup modifies the address space.
		  		              //and if so must modify it.
		assert(ret == 0);
		return;
	}

	vm_addrptr start = (vm_addrptr) ent->begin;
	vm_addrptr end = (vm_addrptr) ent->end;
	void *pa = (void *)dune_va_to_pa((void*) ent->begin);
	unsigned int perm = PERM_NONE;

	printf("0x%016lx - 0x%016lx\n", start, end);
	if (ent->type == PROCMAP_TYPE_VDSO) {
		perm |= PERM_U | PERM_R | PERM_X;
		ret = mm_create_phys_mapping(mm_root, start, end, pa, perm);
		assert(ret == 0);
		return;
	}

	if (ent->type == PROCMAP_TYPE_VVAR) {
		perm |= PERM_U | PERM_R;
		ret = mm_create_phys_mapping(mm_root, start, end, pa, perm);
		assert(ret == 0);
		return;
	}

	if (ent->r)
		perm |= PERM_R;
	if (ent->w)
		perm |= PERM_W;
	if (ent->x)
		perm |= PERM_X;

	/* Other physical mappings.*/
	ret = mm_create_phys_mapping(mm_root, start, end, pa, perm);
	assert(ret == 0);
}

/* Initilises the memory regions.
 * Must be called by dune_memory_init, otherwise will fail.
 * TODO: change the permissions, not sure dune's ones are correct.*/
int mm_init()
{
	int ret;
	void *start = NULL, *end = NULL, *pa = NULL;
	unsigned long perm = 0;
	struct dune_layout layout;
	
	//TODO move this somewhere else.
	// if (ret = ioctl(dune_fd, DUNE_GET_LAYOUT, &layout))
	//  	return ret;
	
	// phys_limit = layout.phys_limit;
	// mmap_base = layout.base_map;
	// stack_base = layout.base_stack;

	/* Map the page base. */
	start = (void*) PAGEBASE;
	end = (void*) (PAGEBASE + MAX_PAGES * PGSIZE);
	pa = (void*) dune_va_to_pa((void*)PAGEBASE);
	perm = PERM_R | PERM_W | PERM_BIG;

	if ((ret = mm_create_phys_mapping(mm_root,(vm_addrptr) start,
		(vm_addrptr) end, pa, perm)))
		return ret;

	/*TODO map the procmap.*/
	dune_procmap_iterate(&__mm_setup_mappings_cb);

	return 0;
}

/* Only colisions expected are complete coverage.
 * If that is not the case, the result will be wrong.
 * This is due to dune puting the physical address in hard
 * for everything that is set using dune_vm_map_phys.
 * TODO refactor.*/
int mm_create_phys_mapping(	mm_struct *mm, 
							vm_addrptr va_start, 
							vm_addrptr va_end, 
							void* pa, 
							unsigned long perm)
{
	int ret = 0;
	vm_area_struct *current = NULL;
	assert(mm->mmap);
	if(!(mm->mmap))
		return -EINVAL;
	/* Fast paths*/
	if (mm->mmap->head == NULL || mm->mmap->head->vm_start >= va_end) {
		vm_area_struct *vma = mm_alloc_vma(mm, va_start, va_end, perm);
		if (!vma)
			return -ENOMEM;
		Q_INSERT_FRONT(mm->mmap, vma, lk_areas);
		ret = mm_apply_to_pgroot_precise(vma, pa);
		return ret;
	}
	
	if (mm->mmap->last->vm_end <= va_start) {
		vm_area_struct *vma = mm_alloc_vma(mm, va_start, va_end, perm);
		if (!vma)
			return -ENOMEM;
		Q_INSERT_TAIL(mm->mmap, vma, lk_areas);
		ret = mm_apply_to_pgroot_precise(vma, pa);
		return ret;
	}

	/* Slow path*/
	Q_FOREACH(current, mm->mmap, lk_areas) {
		if (current->vm_start == va_start &&
			current->vm_end == va_end &&
			current->vm_flags == perm) {
			/* The mapping already exists*/
			return 0;
		}

		if (mm_overlap(current, va_start, va_end)) {
			ret = mm_split_or_merge(mm, current, va_start, va_end, perm, pa);
			break;
		}

		/*Try to insert*/
		if (current->vm_start >=  va_end) {
			vm_area_struct *vma = mm_alloc_vma(mm, va_start, va_end, perm);
			if (!vma)
				return -ENOMEM;
			Q_INSERT_BEFORE(mm->mmap, current, vma, lk_areas);
			ret = mm_apply_to_pgroot_precise(vma, pa);
			break;
		}
	}
	return ret;
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
						void *args)
{
	assert(vma);
	assert(start <= end);
	int ret;
	vm_area_struct *iter = NULL, *last = vma;

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
	vm_area_struct *f_vma = NULL, *s_vma = NULL, *t_vma = NULL;

	/*1.*/
	if (start <= vma->vm_start && end >= last->vm_end) {
		f_vma = mm_alloc_vma(vma->vm_mm, start, end, perm);
		if (!f_vma)
			ret = -ENOMEM;
			goto err;
		goto alloc;
	}

	/*2.*/
	vm_addrptr f_start = (vma->vm_start < start)? vma->vm_start : start;
	vm_addrptr f_end = (vma->vm_start > start)? vma->vm_start : start;
	unsigned long f_perm = (vma->vm_start < start)? vma->vm_flags : perm;
	
	f_vma = mm_alloc_vma(vma->vm_mm, f_start, f_end, f_perm);
	if (!f_vma) {
		ret = -ENOMEM;
		goto err;
	}

	vm_addrptr s_start = f_end;
	vm_addrptr s_end = (vma->vm_end <= end)? vma->vm_end : end;
	unsigned long s_perm = (f_perm == perm)? vma->vm_flags : perm;

	s_vma = mm_alloc_vma(vma->vm_mm, s_start, s_end, s_perm);
	if (!s_vma) {
		ret = -ENOMEM;
		goto err;
	}

	/*3.*/
	vm_addrptr max_end = (vma->vm_end < end)? end : vma->vm_end;
	if (s_end != max_end) {
		vm_addrptr t_start = s_end;
		vm_addrptr t_end = max_end;
		unsigned long t_perm = f_perm;

		t_vma = mm_alloc_vma(vma->vm_mm, t_start, t_end, t_perm);
		if (!t_vma) {
			ret = -ENOMEM;
			goto err;
		}
	}

	//TODO replace with proper addresses.
alloc:
	if (args == NULL)
		goto create;

	/*Map phys is not supposed to generate these types of overlap.*/
	assert(!t_vma);
	assert(!s_vma);
	
	/* Physical address is given.*/
	if ((ret = mm_apply_to_pgroot_precise(f_vma, args)))
		goto err;

	// if (s_vma && (ret = mm_apply_to_pgroot_precise(s_vma, (void *) 0)))
	// 	goto err;
	// if (t_vma && (ret = mm_apply_to_pgroot_precise(t_vma, (void *) 0)))
	// 	goto err;

	goto clean;

create:
	/* Allocate without pre-defined physical address.*/
	if ((ret = mm_apply_to_pgroot(f_vma)))
		goto err;
	if (s_vma && (ret = mm_apply_to_pgroot(s_vma)))
		goto err;
	if (t_vma &&(ret = mm_apply_to_pgroot(t_vma)))
		goto err;
	/* Update the list of vmas.*/
clean:
	vma = vma->lk_areas.prev;
	last = last->lk_areas.next;
	mm_delete_region(mm, vma, last);
	Q_INSERT_AFTER(mm->mmap, vma, f_vma, lk_areas);
	if (s_vma)
		Q_INSERT_AFTER(mm->mmap, f_vma, s_vma, lk_areas);
	if (t_vma)
		Q_INSERT_AFTER(mm->mmap, s_vma, t_vma, lk_areas);

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

int mm_apply_to_pgroot_precise(vm_area_struct *vma, void* pa)
{
	if (!vma || !(vma->vm_mm) || !(vma->vm_mm->pml4))
		return -EINVAL;
	
	//TODO call proper dune page_walk of dune_map_phys or whatev'.
	return dune_vm_map_phys(vma->vm_mm->pml4, (void*)vma->vm_start,
		(size_t)(vma->vm_end - vma->vm_start), pa, vma->vm_flags);
}

int mm_apply_to_pgroot(vm_area_struct *vma)
{
	if (!vma || !(vma->vm_mm) || !(vma->vm_mm->pml4))
		return -EINVAL;
	//TODO call allocator.
	assert(0); //should never be called for the moment.
	return 0;
}

void mm_dump(mm_struct *mm)
{
	assert(mm);
	vm_area_struct *current = NULL;
	Q_FOREACH(current, mm->mmap, lk_areas) {
		printf("0x%016lx - 0x%016lx\n", current->vm_start, current->vm_end);
	}
} 