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
 * WARNING: it does not change the pgroot mappings either.
 * WARNING: addresses shoud be page aligned.*/
static vm_area_struct * mm_alloc_vma(mm_struct *mm,
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

/* Frees a vma and handles the shared and cow list.
 * NOTE: the page root should be modified somewhere else.*/
static void mm_delete_vma(vm_area_struct *vma)
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
}

/* Removes the vmas between start (not included) and end (not included) for the mm->mmap.
 * Frees the removed vmas.
 * If start == NULL start at head.
 * If end == NULL ends after last.
 * WARNING: does not affect the page tables.
 * SHOULD it? Maybe implement the functionality elsewhere.
 * TODO: Should optimize, instead of removing, resize in split or merge.*/
static int mm_delete_region(mm_struct *mm,
							vm_area_struct *start,
							vm_area_struct *end)
{
	vm_area_struct *iter = (!start)? mm->mmap->head : start->lk_areas.next;
	while (iter != NULL) {
		vm_area_struct *tmp = iter;
		iter = iter->lk_areas.next;
		
		Q_REMOVE(mm->mmap, tmp, lk_areas);
		mm_delete_vma(tmp);
		if (iter == end)
			break;
	}

	return 0;
}

#define VSYSCALL_ADDR 0xffffffffff600000

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
		assert(ret == 0);
		return;
	}

	vm_addrptr start = (vm_addrptr) ent->begin;
	vm_addrptr end = (vm_addrptr) ent->end;
	void *pa = (void *)dune_va_to_pa((void*) ent->begin);
	unsigned int perm = PERM_NONE;

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

/* Init the memory regions.
 * Must be called by dune_memory_init, otherwise will fail.
 * TODO: change the permissions, not sure dune's ones are correct.*/
int mm_init()
{
	int ret;
	void *start = NULL, *end = NULL, *pa = NULL;
	unsigned long perm = 0;

	/* Map the page base. */
	start = (void*) PAGEBASE;
	end = (void*) (PAGEBASE + MAX_PAGES * PGSIZE);
	pa = (void*) dune_va_to_pa((void*)PAGEBASE);
	perm = PERM_R | PERM_W | PERM_BIG;

	 if ((ret = mm_create_phys_mapping(mm_root,(vm_addrptr) start,
	 	(vm_addrptr) end, pa, perm)))
	 	return ret;

	/*Map the procmap.*/
	dune_procmap_iterate(&__mm_setup_mappings_cb);
	return 0;
}

/* TODO: refactor.
 * TODO: lack of merging when creating a new vma*/
int mm_create_phys_mapping(mm_struct *mm, 
							vm_addrptr va_start, 
							vm_addrptr va_end, 
							void* pa, 
							unsigned long perm)
{
	assert(mm->mmap); assert(va_start < va_end);
	
	int ret = 0;
	vm_area_struct *current = NULL;

	/* Make sure the addresses are page aligned*/
	va_start = MM_PGALIGN_DN(va_start);
	va_end = MM_PGALIGN_UP(va_end);
	
	if(!(mm->mmap))
		return -EINVAL;
	/* Fast paths*/
	if (mm->mmap->head == NULL || mm->mmap->head->vm_start >= va_end) {
		vm_area_struct *vma = mm_alloc_vma(mm, va_start, va_end, perm);
		if (!vma)
			return -ENOMEM;
		Q_INSERT_FRONT(mm->mmap, vma, lk_areas);
		ret = mm_apply_to_pgroot(vma, pa);
		return ret;
	}
	
	if (mm->mmap->last->vm_end <= va_start) {
		vm_area_struct *vma = mm_alloc_vma(mm, va_start, va_end, perm);
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
			return 0;
		}

		if (mm_overlap(current, va_start, va_end)) {
			ret = mm_split_or_merge(mm, current, va_start, va_end, perm, 
				&mm_apply_to_pgroot, pa);

			return ret;
		}

		/*Try to insert*/
		if (current->vm_start >=  va_end) {
			vm_area_struct *vma = mm_alloc_vma(mm, va_start, va_end, perm);
			if (!vma)
				return -ENOMEM;
			Q_INSERT_BEFORE(mm->mmap, current, vma, lk_areas);
			ret = mm_apply_to_pgroot(vma, pa);
			return ret;
		}
	}
	/*Should never come here.*/
	assert(0);
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
						mm_cb_ft f,
						void *args)
{
	assert(vma);
	assert(start <= end);
	int ret;
	vm_area_struct *iter = NULL, *last = NULL;

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
		f_vma = mm_alloc_vma(vma->vm_mm, start, end, perm);
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

	t_vma = mm_alloc_vma(vma->vm_mm, t_start, t_end, t_perm);
	if (!t_vma) {
		ret = -ENOMEM;
		goto err;
	}

create12:
	f_vma = mm_alloc_vma(vma->vm_mm, f_start, f_end, f_perm);
	if (!f_vma) {
		ret = -ENOMEM;
		goto err;
	}

	s_vma = mm_alloc_vma(vma->vm_mm, s_start, s_end, s_perm);
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

int mm_apply_to_pgroot(vm_area_struct *vma, void *pa)
{
	if (!vma || !(vma->vm_mm) || !(vma->vm_mm->pml4))
		return -EINVAL;
	if (pa != NULL) {
		return dune_vm_map_phys(vma->vm_mm->pml4, (void*)vma->vm_start,
		(size_t)(vma->vm_end - vma->vm_start), pa, vma->vm_flags);
	}

	//TODO: allocate new page.
	//FIXME: should never be called for the moment.
	assert(0);
	return 1;
}

void mm_dump(mm_struct *mm)
{
	assert(mm);
	vm_area_struct *current = NULL;
	Q_FOREACH(current, mm->mmap, lk_areas) {
		printf("0x%016lx-0x%016lx: %016lx\n", current->vm_start, 
			current->vm_end, current->vm_flags);
	}
}

void mm_verify_mappings(mm_struct *mm)
{
	assert(mm);
	assert(mm->mmap);
	assert(pgroot == mm->pml4);
	vm_area_struct *current = NULL;
	Q_FOREACH(current, mm->mmap, lk_areas) {
		int mapped = dune_vm_has_mapping(mm->pml4, (void *) current->vm_start);
		vm_addrptr middle = (current->vm_start + current->vm_end) / 2;
		mapped += dune_vm_has_mapping(mm->pml4, (void *) middle);
		assert(mapped == 0);
	}
}

static int __mm_apply_protect(vm_area_struct *vma, void* perm)
{
	assert(perm != NULL);
	if (!vma || !(vma->vm_mm) || !(vma->vm_mm->pml4))
		return -EINVAL;

	assert(vma->vm_flags == *((unsigned long*)perm));
	return dune_vm_mprotect(vma->vm_mm->pml4,(void*)(vma->vm_start),
		(size_t)(vma->vm_end - vma->vm_start), vma->vm_flags);
}

/* Modifies the permissions for the vmas that map the provided range.
 * If the virtual memory region is not mapped, it is NOT created.
 * If the start or end address is within a vma, we split it (if possible).*/
int mm_mprotect(mm_struct *mm, vm_addrptr start,
				vm_addrptr end, unsigned long perm)
{
	assert(mm);
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
			ret = mm_split_or_merge(mm, current, start, end, perm, 
				&__mm_apply_protect, &perm);
			return ret;
		}
	}
	
	/* Should never come here.*/
	//TODO: remove this once we check it's correct.
	assert(0); 
	return -EINVAL;
}

static int __mm_unmap(vm_area_struct *vma, void *args)
{
	assert(vma != NULL);
	vm_area_struct **pt = ((vm_area_struct**)args);
	*pt = vma;
	return 0;
}

int mm_unmap(mm_struct *mm, vm_addrptr start, vm_addrptr end, bool apply)
{
	assert(start < end);
	int ret = 0;
	vm_area_struct *current = NULL, *to_rm = NULL;
	
	/* Align the addresses.*/
	start = MM_PGALIGN_DN(start);
	end = MM_PGALIGN_UP(end);

	Q_FOREACH(current, mm->mmap, lk_areas) {
		if (mm_overlap(current, start, end)) {
			ret = mm_split_or_merge(mm, current, start, end, 0, 
				&__mm_unmap, &to_rm);
			
			if (ret)
				return ret;
			
			assert(to_rm != NULL);
			Q_REMOVE(mm->mmap, to_rm, lk_areas);
		
			if (apply) {
				//FIXME: problem here, what if was COW or shared?
				dune_vm_unmap(mm->pml4, (void*)(to_rm->vm_start), 
					(size_t)(to_rm->vm_end - to_rm->vm_start));
			}
			//FIXME: same problem as above.
			free(to_rm);
			
			return ret;
		}
	}
	/* TODO: for debugging remove afterwards.
	 * Need to decide if we return 0 or it is a fault to unmap invalid addr.*/
	assert(0);
	return -EINVAL;
}

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

/* Creates a new vma initialized only with access rights, start and end.*/
static vm_area_struct* mm_copy_vma(vm_area_struct *vma)
{
	assert(vma);
	vm_area_struct *copy = malloc(sizeof(vm_area_struct));
	if (!copy)
		goto err;
	
	memcpy(copy, vma, sizeof(vm_area_struct));
	Q_INIT_ELEM(copy, lk_areas);
	Q_INIT_ELEM(copy, lk_shared);
	copy->dirty = 1;
	copy->vm_mm = NULL;
	copy->head_shared = NULL;

	return copy;
err:
	return NULL;
}

/* Create a cow copy of the vma.*/
static vm_area_struct* mm_cow_copy_vma(vm_area_struct *vma)
{
	vm_area_struct *copy = mm_copy_vma(vma);
	if (!copy)
		goto err;
	l_vm_area *shared = NULL;
	
	if (vma->cow) {
		assert(vma->head_shared);
		assert(vma->vm_flags & PERM_COW);
		shared = vma->head_shared;
	} else {
		shared = malloc(sizeof(l_vm_area));
		if (!shared)
			goto err;
		Q_INIT_HEAD(shared);
		Q_INSERT_TAIL(shared, vma, lk_shared);
		vma->head_shared = shared;
		vma->cow = 1;
		vma->vm_flags |= PERM_COW;
		vma->dirty = 1;
	}

	copy->head_shared = shared;
	copy->cow = 1;
	copy->dirty = 1;
	Q_INSERT_TAIL(shared, copy, lk_shared);

	return copy;
err:
	if (copy)
		free(copy);
	return NULL;
}

/* Creates a shared copy of the vma.*/
static vm_area_struct* mm_shared_copy_vma(vm_area_struct *vma)
{
	assert(vma);
	vm_area_struct *copy = mm_copy_vma(vma);
	if (!copy)
		goto err;
	l_vm_area *shared = NULL;

	if (vma->shared && vma->head_shared) {
		assert(vma->head_shared);
		shared = vma->head_shared;
	} else {
		shared = malloc(sizeof(l_vm_area));
		if (!shared)
			goto err;
		Q_INIT_HEAD(shared);
		Q_INSERT_TAIL(shared, vma, lk_shared);
		vma->head_shared = shared;
		vma->shared = 1;
		vma->dirty = 1;
	}

	copy->head_shared = shared;
	copy->shared = 1;
	copy->dirty = 1;
	Q_INSERT_TAIL(shared, copy, lk_shared);

	return copy;
err:
	if (copy)
		free(copy);
	return NULL;
}

/* Creates a copy on write copy of the mm. The kernel space is shared.*/
mm_struct* mm_cow_copy(mm_struct *mm, bool apply)
{
	assert(mm && mm->mmap);
	vm_area_struct *current = NULL;
	mm_struct *copy = malloc(sizeof(mm_struct));
	if (!copy)
		goto err;
	copy->mmap = malloc(sizeof(l_vm_area));
	if (!(copy->mmap))
		goto err;

	copy->pml4 = memalign(PGSIZE, PGSIZE);
	
	if (!(copy->pml4))
		goto err;
	
	memset(copy->pml4, 0, PGSIZE);
	Q_INIT_ELEM(copy, lk_mms);
	Q_FOREACH(current, mm->mmap, lk_areas) {
		vm_area_struct *vmcpy = NULL;
		if (!(current->user)) {
			vmcpy = mm_shared_copy_vma(current);
		} else {
			vmcpy = mm_cow_copy_vma(current);
		}
		if (!vmcpy)
			goto err;
		vmcpy->vm_mm = copy;
		Q_INSERT_TAIL(copy->mmap, vmcpy, lk_areas);
	}
	//TODO: apply to page root.
	if (apply) {
		mm_apply(mm);
		mm_apply(copy);
	}

	return copy;
err:
	if (copy)
		mm_free(copy);
	return NULL;
}

int mm_free(mm_struct *mm)
{
	assert(mm);
	//TODO: free all vmas, give back pages, free mm.
	//TODO check the ref in the mm...
	int ret = 0;
	vm_area_struct *iter = mm->mmap->head;
	while(iter != NULL) {
		vm_area_struct *current = iter;
		iter = iter->lk_areas.next;

		//TODO: free memory if possible.
		if (!(current->cow || current->shared)) {
			dune_vm_unmap(mm->pml4, (void*)(current->vm_start),
				(size_t)(current->vm_end - current->vm_end));
		}
		mm_delete_vma(current);
	}
	//FIXME: free pml4 etc.
	return ret;
}

void mm_apply(mm_struct *mm)
{
	vm_area_struct *current = NULL;
	Q_FOREACH(current, mm->mmap, lk_areas) {
		if (current->user == 0)
			continue;
		if (current->dirty)
			__mm_apply_protect(current, mm->pml4);
	}
}