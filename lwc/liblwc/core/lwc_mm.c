#include <stdio.h>
#include <errno.h>
#include <mm/memory.h>
#include <mm/vm_tools.h>

#include "lwc_mm.h"



static vm_area_struct* __lwc_mm_last_vma(vm_area_struct* vma, lwc_rg_struct *rg)
{
	vm_area_struct *previous = vma, *current = vma->lk_areas.next;
	
	while (current && mm_overlap(current, rg->start, rg->end)) {
		previous = current;
		current = current->lk_areas.next;
	}

	return previous;
}

static int __lwc_validate_mod(vm_area_struct *vma, void* args)
{
    ASSERT_DBG(vma, "vma is null.\n");
    if (vma->vm_flags & PERM_U)
        return 0;
    return 1;
}

static int lwc_validate_mod(lwc_rsrc_spec *mod, mm_struct *o)
{
    ASSERT_DBG(o, "o is null.\n");
    ASSERT_DBG(mod, "mod is null.\n");

    lwc_rg_struct *prev = NULL, *curr = NULL;
    Q_FOREACH(curr, &(mod->ranges), lk_rg) {
        if ((prev && !(prev->end <= prev->start)) ||
            !(curr->start <= curr->end)) {
            /* The ranges are not increasing.*/
            return 1;
        }

        //FIXME: doesn't work.
        vm_area_struct *start = mm_find(o, curr->start, false);
        vm_area_struct *end = mm_find(o, curr->end -1, true);
        
        if (!start || !end) {
            return 2;
        }

        if (start != end) {
            ASSERT_DBG(start->vm_end <= end->vm_start,
                "start->vm_end{0x%016lx}, end->vm_start{0x%016lx}\n",
                start->vm_end, end->vm_start);
        }
        
        /*Check that none is kernel.*/
        if (mm_vmas_walk(start, end, &__lwc_validate_mod, NULL))
            return 4;
        
        /* Update prev.*/
        prev = curr;
    }

    return 0;
}

static int __share_mem_helper(ptent_t *pte, void *va, cb_info *args)
{
	ASSERT_DBG(pte && args, "pte{%p}, args{%p}\n", pte, args);
    int ret;
    ptent_t* pte_c = NULL;
    mm_struct *copy = (mm_struct*)(args->args);
    ret = vm_lookup(copy->pml4, va, &pte_c, CREATE_NONE, 0);
    if (ret != 0) {
        return ret;
    }

    /* Remap the page in the copy.*/
    ASSERT_DBG(*pte & PTE_U, "user permissions missing.\n");
    ASSERT_DBG(*pte & PTE_P, "pte not present.\n");
    ASSERT_DBG(!(*pte & PTE_COW), "pte is cow.\n");
    ASSERT_DBG(*pte_c & PTE_U, "copy missing user permissions.\n");
    ASSERT_DBG(*pte_c & PTE_P, "copy not present.\n");
    ASSERT_DBG(*pte_c & PTE_COW, "pte_c not cow.\n");
    ASSERT_DBG(!(*pte_c & PTE_W), "pte_c has write permissions.\n");
    struct page *pg = dune_pa2page(PTE_ADDR(*pte_c));
    
    if (dune_page_isfrompool(PTE_ADDR(*pte_c)))
        dune_page_put(pg);

    *pte_c = *pte;

    return ret;
}

static int __lwc_shared(mm_struct *o, 
						mm_struct* copy,
						lwc_rg_struct *rg,
						vm_area_struct *start,
						vm_area_struct *end)
{
	int ret = 0;
	vm_addrptr s = MM_PGALIGN_DN(rg->start);
	vm_addrptr e = MM_PGALIGN_UP(rg->end);

	/* Uncow in the parent.*/
	vm_addrptr curr;
	for (curr = s; curr < e; curr += PGSIZE) {
		mm_uncow(o, curr);
	}

	/* Copy the vmas in the child.*/
	vm_area_struct *current = NULL;
	for (current = start; current != NULL; current = current->lk_areas.next) {
		vm_area_struct *vmacpy = vma_copy(current, false);
		if (!vmacpy) goto err;

		vmacpy->vm_mm = copy;
		Q_INSERT_TAIL(copy->mmap, vmacpy, lk_areas);

		if (current == end)
			break;
	}
	
	ret = vm_pgrot_walk(o->pml4, (void*) (rg->start), (void*)(rg->end-1),
		&__share_mem_helper, NULL, copy);

	return ret;
err:
	return -EINVAL;
}


static int __lwc_unmap(	mm_struct *o,
						mm_struct *copy,
						lwc_rg_struct *rg,
						vm_area_struct *start,
						vm_area_struct *end)
{
	vm_addrptr s = MM_PGALIGN_DN(rg->start);
	vm_addrptr e = MM_PGALIGN_UP(rg->end);

	/* First one is split.*/
	if (start && start->vm_start < s) {
		//TODO aghosn: put asserts that s is within first vma.
		vm_area_struct *fst = vma_create(copy, start->vm_start, s, start->vm_flags);
		if (!fst) goto err;

		Q_INSERT_TAIL(copy->mmap, fst, lk_areas);
	}

	/* Last one is split.*/
	if (end && end->vm_end > e) {
		vm_area_struct *lst = vma_create(copy, e, end->vm_end, end->vm_flags);
		if (!lst) goto err;

		Q_INSERT_TAIL(copy->mmap, lst, lk_areas);
	}

	/* Unmap in the pml4.*/
	dune_vm_unmap(copy->pml4, (void*) s, (size_t)(e - s));

	return 0;

err:
	return -EINVAL;
}


static int __ro_mem_helper(ptent_t *pte, void *va, cb_info *args)
{
	ASSERT_DBG(pte && args, "pte{%p}, args{%p}\n", pte, args);
    int ret;
    ptent_t* pte_c = NULL;
    mm_struct *copy = (mm_struct*)(args->args);
    ret = vm_lookup(copy->pml4, va, &pte_c, CREATE_NONE, 0);
    if (ret != 0) {
        return ret;
    }

    /* Remap the page in the copy.*/
    ASSERT_DBG(*pte & PTE_U, "user permissions missing.\n");
    ASSERT_DBG(*pte & PTE_P, "pte not present.\n");
    ASSERT_DBG(!(*pte & PTE_COW), "pte is cow.\n");
    ASSERT_DBG(*pte_c & PTE_U, "copy missing user permissions.\n");
    ASSERT_DBG(*pte_c & PTE_P, "copy not present.\n");
    ASSERT_DBG(*pte_c & PTE_COW, "pte_c not cow.\n");
    ASSERT_DBG(!(*pte_c & PTE_W), "pte_c has write permissions.\n");
    struct page *pg = dune_pa2page(PTE_ADDR(*pte_c));
    
    if (dune_page_isfrompool(PTE_ADDR(*pte_c)))
        dune_page_put(pg);

    *pte_c = *pte & ~(PTE_W);

    return ret;
}

static int __lwc_ro(mm_struct *o,
					mm_struct *copy,
					lwc_rg_struct *rg,
					vm_area_struct *start,
					vm_area_struct *end)
{
	vm_addrptr s = MM_PGALIGN_DN(rg->start);
	vm_addrptr e = MM_PGALIGN_UP(rg->end);

	/* Split first*/
	if (start && start->vm_start < s) {
		vm_area_struct *fst = vma_create(copy, start->vm_start, s, start->vm_flags);
		if (!fst) goto err;

		Q_INSERT_TAIL(copy->mmap, fst, lk_areas);
	}

	/* Uncow the parent.*/
	for (vm_addrptr curr = s; curr < e; curr += PGSIZE) {
		mm_uncow(o, curr);
	}

	/* read only in the child.*/
	vm_addrptr nfsts = (start->vm_start < s)? s : start->vm_start;
	
	vm_area_struct *h = vma_create(copy, nfsts, start->vm_end, start->vm_flags & (~PERM_W));
	if (!h) goto err;

	Q_INSERT_TAIL(copy->mmap, h, lk_areas);

	if (start == end) goto finish;

	vm_area_struct *curr = NULL;
	for (curr = start->lk_areas.next; curr != NULL; curr = curr->lk_areas.next){
		vm_addrptr svma = curr->vm_start;
		vm_addrptr evma = (curr == end)? e : curr->vm_end;
		unsigned long flags = curr->vm_flags & (~PERM_W);

		vm_area_struct *vma = vma_create(copy, svma, evma, flags);
		if (!vma) goto err;

		Q_INSERT_TAIL(copy->mmap, vma, lk_areas);
		if (curr == end)
			break;
	}

finish:
	/* Split the last*/
	if (end && end->vm_end > e) {
		vm_area_struct *lst = vma_create(copy, e, end->vm_end, end->vm_flags);
		if (!lst) goto err;

		Q_INSERT_TAIL(copy->mmap, lst, lk_areas);
	}

	
	return vm_pgrot_walk(o->pml4, (void*) (s), (void*)(e-1), 
						&__ro_mem_helper, NULL, copy);
err:
	return -EINVAL;
}

mm_struct* lwc_mm_create(mm_struct *o, lwc_rsrc_spec *mod)
{
	ASSERT_DBG(o && mod, "o{%p}, mod{%p}.\n", o, mod);
	mm_struct *copy = NULL;
	
	vm_area_struct *current = NULL, *vma = NULL;
	lwc_rg_struct *range = mod->ranges.head;

	if (lwc_validate_mod(mod, o))
		goto err;

	/*Create the new memory mapping.*/
	copy = malloc(sizeof(mm_struct));
	if (!copy) goto err;

	Q_INIT_ELEM(copy, lk_mms);
	Q_INIT_HEAD(copy->mmap);

	/*Copy the pml4*/
	copy->pml4 = vm_pgrot_copy(o->pml4, true);
	if (!(copy->pml4)) goto err;


	Q_FOREACH(current, o->mmap, lk_areas) {
		if (range && mm_overlap(current, range->start, range->end)) {
			vm_area_struct *end = __lwc_mm_last_vma(current, range);

			switch(range->opt) {
				case LWC_COW:
					range = range->lk_rg.next;
					goto cow;
					break;
				case LWC_SHARED:
					__lwc_shared(o, copy, range, current, end);
					break;
				case LWC_UNMAP:
					__lwc_unmap(o, copy, range, current, end);
					break;
				case LWC_RO:
					__lwc_ro(o, copy, range, current, end);
					break;
				default:
					goto err;
			}
			range = range->lk_rg.next;
			current = (end)? end->lk_areas.next : NULL;
		} else {
cow:		//TODO should make cow?
			vma = vma_create(copy, current->vm_start, current->vm_end, 
															current->vm_flags);
			if (!vma) goto err;

			Q_INSERT_TAIL(copy->mmap, vma, lk_areas);
		}
	}

	return copy;

err:
	return NULL;
}