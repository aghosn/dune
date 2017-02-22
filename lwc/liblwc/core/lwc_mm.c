#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <mm/memory.h>
#include <mm/vm_tools.h>

#include "lwc_mm.h"



static vm_area_struct* __lwc_mm_last_vma(vm_area_struct* vma, lwc_rg_struct *rg)
{
	vm_area_struct *previous = vma, *current = TAILQ_NEXT(vma, q_areas);
	
	while (current && mm_overlap(current, rg->start, rg->end)) {
		previous = current;
		current = TAILQ_NEXT(current, q_areas);
	}

	return previous;
}

static int lwc_validate_mod(lwc_rg_struct *mod, unsigned int numr, mm_struct *o)
{
    ASSERT_DBG(o, "o is null.\n");
    ASSERT_DBG(mod, "mod is null.\n");

    /* No modifiers*/
    if (numr == 0)
    	return 0;

    lwc_rg_struct *prev = NULL, curr;
    for (int i = 0; i < numr; i++) {
    	curr = mod[i];
        if ((prev && !(prev->end <= curr.start)) ||
            !(curr.start <= curr.end)) {
            /* The ranges are not sorted in increasing order.*/
            return 1;
        }

        vm_area_struct *start = mm_find(o, curr.start, false);
        vm_area_struct *end = mm_find(o, curr.end -1, true);
        
        if (!start || !end) {
            return 2;
        }

        if (start != end) {
            ASSERT_DBG(start->vm_end <= end->vm_start,
                "start->vm_end{0x%016lx}, end->vm_start{0x%016lx}\n",
                start->vm_end, end->vm_start);
        }
        
        /*Check that none is kernel.*/
        vm_area_struct *current = NULL;
       	for (current = start;
       		current != NULL;
       		current = TAILQ_NEXT(current, q_areas)) {

       		ASSERT_DBG(current->vm_flags & PERM_U, "Trying to modify kernel.\n");
       		if (current == end)
       			break;
       	}
        
        /* Update prev.*/
        prev = &curr;
    }

    return 0;
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

	vm_area_struct *current = NULL;
	for (current = start;
		current != NULL; 
		current = TAILQ_NEXT(current, q_areas)) {

		vm_area_struct *vmacpy = vma_copy(current, false);
		if (!vmacpy) goto err;

		vmacpy->vm_mm = copy;
		TAILQ_INSERT_TAIL(&(copy->mmap), vmacpy, q_areas);

		if (current == end)
			break;
	}


	if (!vm_pgrot_copy_range(o->pml4, copy->pml4,
		(void*) s, (void*) e, false, CB_SHARE)) {
		goto err;
	}

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
		ASSERT_DBG(s > start->vm_start && s < start->vm_end, 
			"The aligned start address is not correct.\n");
		vm_area_struct *fst = vma_create(copy, start->vm_start, s, start->vm_flags);
		if (!fst) goto err;

		TAILQ_INSERT_TAIL(&(copy->mmap), fst, q_areas);
		if (!vm_pgrot_copy_range(o->pml4, copy->pml4, (void*) fst->vm_start,
			(void*) fst->vm_end, true, CB_COW)) {
			goto err;
		}
	}

	/* Last one is split.*/
	if (end && end->vm_end > e) {
		vm_area_struct *lst = vma_create(copy, e, end->vm_end, end->vm_flags);
		if (!lst) goto err;

		TAILQ_INSERT_TAIL(&(copy->mmap), lst, q_areas);
		if (!vm_pgrot_copy_range(o->pml4, copy->pml4, (void*) lst->vm_start,
			(void*) lst->vm_end, true, CB_COW)) {
			goto err;
		}
	}

	return 0;

err:
	return -EINVAL;
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

		TAILQ_INSERT_TAIL(&(copy->mmap), fst, q_areas);
		if (!vm_pgrot_copy_range(o->pml4, copy->pml4, (void*)fst->vm_start,
			(void*)fst->vm_end, true, CB_COW)) {
			goto err;
		}
	}

	/* read only in the child.*/
	vm_addrptr nfsts = (start->vm_start < s)? s : start->vm_start;
	
	vm_area_struct *h = vma_create(copy, nfsts, start->vm_end, start->vm_flags & (~PERM_W));
	if (!h) goto err;

	TAILQ_INSERT_TAIL(&(copy->mmap), h, q_areas);
	if (!vm_pgrot_copy_range(o->pml4, copy->pml4, (void*) h->vm_start,
		(void*) h->vm_end, false, CB_RO)) {
		goto err;
	}

	if (start == end) goto finish;

	vm_area_struct *curr = NULL;
	for (curr = TAILQ_NEXT(start, q_areas);
		curr != NULL;
		curr = TAILQ_NEXT(curr, q_areas)){

		vm_addrptr svma = curr->vm_start;
		vm_addrptr evma = (curr == end)? e : curr->vm_end;
		unsigned long flags = curr->vm_flags & (~PERM_W);

		vm_area_struct *vma = vma_create(copy, svma, evma, flags);
		if (!vma) goto err;

		TAILQ_INSERT_TAIL(&(copy->mmap), vma, q_areas);
		if (!vm_pgrot_copy_range(o->pml4, copy->pml4, (void*) vma->vm_start,
			(void*) vma->vm_end, false, CB_RO)) {
			goto err;
		}
		if (curr == end)
			break;
	}

finish:
	/* Split the last*/
	if (end && end->vm_end > e) {
		vm_area_struct *lst = vma_create(copy, e, end->vm_end, end->vm_flags);
		if (!lst) goto err;

		TAILQ_INSERT_TAIL(&(copy->mmap), lst, q_areas);
		if (!vm_pgrot_copy_range(o->pml4, copy->pml4, (void*) lst->vm_start,
			(void*) lst->vm_end, false, CB_RO)) {
			goto err;
		}
	}
	
	return 0;
err:
	return -EINVAL;
}

mm_struct* lwc_mm_create(mm_struct *o, lwc_rg_struct *mod, unsigned int numr)
{
	ASSERT_DBG(o && mod, "o{%p}, mod{%p}.\n", o, mod);
	mm_struct *copy = NULL;
	
	vm_area_struct *current = NULL, *vma = NULL;
	//lwc_rg_struct *range = mod->ranges.head;
	int index = 0;

	if (lwc_validate_mod(mod, numr, o))
		goto err;

	/*Create the new memory mapping.*/
	copy = malloc(sizeof(mm_struct));
	if (!copy) goto err;

	
	TAILQ_INIT(&(copy->mmap));

	/*Copy the pml4*/
	copy->pml4 = memalign(PGSIZE, PGSIZE);
	if (!(copy->pml4)) goto err;

	memset(copy->pml4, 0, PGSIZE);

	TAILQ_FOREACH(current, &(o->mmap), q_areas) {
		lwc_rg_struct range;

loop_beg:
		if (index < numr)		
			range = mod[index];
		
		if (index < numr && mm_overlap(current, range.start, range.end)) {
			vm_area_struct *end = __lwc_mm_last_vma(current, &range);

			switch(range.opt) {
				case LWC_COW:
					index++;
					goto cow;
					break;
				case LWC_SHARED:
					__lwc_shared(o, copy, &range, current, end);
					break;
				case LWC_UNMAP:
					__lwc_unmap(o, copy, &range, current, end);
					break;
				case LWC_RO:
					__lwc_ro(o, copy, &range, current, end);
					break;
				default:
					goto err;
			}
			index++;
			current = (end)? TAILQ_NEXT(end, q_areas) : NULL;

			/* Need that to continue with the proper vma.*/
			if (end != NULL) goto loop_beg;
		} else {
cow:		
			vma = vma_create(copy, current->vm_start, current->vm_end, 
															current->vm_flags);

			if (!vma) goto err;

			TAILQ_INSERT_TAIL(&(copy->mmap), vma, q_areas);
			if (!vm_pgrot_copy_range(o->pml4, copy->pml4, (void*) vma->vm_start,
				(void*) vma->vm_end, true, CB_COW)) {
				goto err;
			}
		}
	}

	return copy;

err:
	return NULL;
}