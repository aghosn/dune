#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <stdbool.h>

#include <dune.h>

#include "mm.h"
#include "mm_types.h"

//Keeping track of all vmas.
l_vm_areas_t *vm_areas = NULL;

//The current memory mapping.
mm_t* mm_current = NULL;

//TODO
static vm_area_t* create_vm_area(uint64_t start, uint64_t end, unsigned long flags) {
	assert(start <= end);
	//TODO check also that both start and end are page aligned.
	vm_area_t *result = malloc(sizeof(vm_area_t));
	//Allocation failed.
	if (!result)
		return NULL;
	result->vm_start = start;
	result->vm_end = end;
	result->vm_flags = flags;

	//Keep track of the vma created.
	Q_INSERT_TAIL(vm_areas, result, lk_vmas);
	return result;
}

//TODO
static wrap_vm_area_t* wrap_area(vm_area_t* area) {
	assert(area != NULL);

	wrap_vm_area_t* result = malloc(sizeof(wrap_vm_area_t));
	if (!result)
		return NULL;

	//TODO: in multithreaded, need this to be atomic.
	result->vm_area = area;
	area->ref++;

	Q_INIT_ELEM(result, lk_lwc);
	return result;
}

static void mm_add_area(mm_t* mm, uint64_t start, uint64_t end, unsigned long flags) {
	vm_area_t *area = create_vm_area(start, end, flags);
	assert(area);
	wrap_vm_area_t *wrapper = wrap_area(area);
	assert(wrapper);

	Q_INSERT_TAIL(&(mm->mmap), wrapper, lk_lwc);
}

mm_t* mm_init() {
	
	vm_areas = malloc(sizeof(l_vm_areas_t));
	Q_INIT_HEAD(vm_areas);

	if (vm_areas == NULL)
		return NULL; //TODO proper error.
	
	/*Initialize the current mm_t*/
	mm_t *mm = malloc(sizeof(mm_t));
	assert(mm);
	mm->pgroot = pgroot;
	//current->mmap.head = current->mmap.last = NULL;
	Q_INIT_HEAD(&(mm->mmap));

	ptent_t* pml4 = pgroot, *pdpte, *pde, *pte;
	
	unsigned long flags = 0;
	uint64_t va_start = 0;
	uint64_t va_end = 0;
	bool in_vma = false;

	for (int i = 0; i < NPTENTRIES; i++) {
		if (!pte_present(pml4[i])) {
			if (in_vma) {
				mm_add_area(mm, va_start, va_end, flags);
				in_vma = false;
			}
			continue;
		}

		if (in_vma && PTE_FLAGS(pml4[i]) != flags) {
			mm_add_area(mm, va_start, va_end, flags);
			in_vma = false;
		}

		pdpte = (ptent_t*) PTE_ADDR(pml4[i]);
		for (int j = 0; j < NPTENTRIES; j++) {
			if (!pte_present(pdpte[j])) {
				if (in_vma) {
					mm_add_area(mm, va_start, va_end, flags);
					in_vma = false;
				}
				continue;
			}

			//TODO check this.
			if (in_vma && PTE_FLAGS(pdpte[j]) != flags) {
				mm_add_area(mm, va_start, va_end, flags);
				in_vma = false;
			}

			if (pte_big(pdpte[j])) {
				if (!in_vma) {
					va_start = RPDX(i, j, 0, 0);
					flags = PTE_FLAGS(pdpte[j]);
					in_vma = true;
				}
				va_end = RPDX(i, j, 0, 0) + PGSIZE_1GB;
				continue;
			}

			pde = (ptent_t*) PTE_ADDR(pdpte[j]);
			for (int k = 0; k < NPTENTRIES; k++) {
				if (!pte_present(pde[k])) {
					if (in_vma) {
						mm_add_area(mm, va_start, va_end, flags);
						in_vma = false;
					}
					continue;
				}

				if (in_vma && PTE_FLAGS(pde[k]) != flags) {
					mm_add_area(mm, va_start, va_end, flags);
					in_vma = false;
				}

				if (pte_big(pde[k])) {
					if (!in_vma) {
						va_start = RPDX(i, j, k, 0);
						flags = PTE_FLAGS(pde[k]);
						in_vma = true;
					}
					va_end = RPDX(i, j, k, 0) + PGSIZE_2MB;
					continue;
				}

				pte = (ptent_t*) PTE_ADDR(pde[k]);
				for (int l= 0; l < NPTENTRIES; l++) {
					if (!pte_present(pte[l])) {
						if (in_vma) {
							mm_add_area(mm, va_start, va_end, flags);
							in_vma = false;
						}
						continue;
					}

					if (in_vma && PTE_FLAGS(pte[l]) != flags) {
						mm_add_area(mm, va_start, va_end, flags);
						in_vma = false;
					}

					if (!in_vma) {
						va_start = RPDX(i, j, k, l);
						flags = PTE_FLAGS(pte[l]);
						in_vma = true;
					}
					va_end = RPDX(i, j, k, l) + PGSIZE_4K;
				}
			}
		}

	}
	//TODO can compare with procmap at the end.
	//Just dump it and dune_dump_procmap.
	return mm;
}

void mm_dump_mm(mm_t* mm) {
	wrap_vm_area_t *current = NULL;
	Q_FOREACH(current, &(mm->mmap), lk_lwc) {
		printf("0x%016lx-0x%016lx", current->vm_area->vm_start, current->vm_area->vm_end);
		if (current->vm_area->vm_flags & (1 << 2))
			printf(" U");
		else 
			printf(" S");
		if (current->vm_area->vm_flags & (1 << 1))
			printf(" R/W\n");
		else 
			printf("R/-\n");
	}
}
