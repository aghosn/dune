#ifndef __LWC_LIBLWC_VM_MM_H__
#define __LWC_LIBLWC_VM_MM_H__

#include "mm_types.h"

/*Returns the virtual address corresponding to these indices.*/
#define UL(i)	((uint64_t) i)
#define RPDX(i, j, k, l) \
(uintptr_t)((UL(i) << PDSHIFT(3)) | (UL(j) << PDSHIFT(2)) \
	| (UL(k) << PDSHIFT(1)) | (UL(l) << PDSHIFT(0)))

#define PGSIZE_4K (1 << PGSHIFT)
#define PGSIZE_2MB (1 << (PGSHIFT + NPTBITS))
#define PGSIZE_1GB (1 << (PGSHIFT + NPTBITS + NPTBITS))


static inline int pte_present(ptent_t pte)
{
	return (PTE_FLAGS(pte) & PTE_P);
}

static inline int pte_big(ptent_t pte)
{
	return (PTE_FLAGS(pte) & PTE_PS);
}
/******************************************************************************/

extern l_vm_areas_t *vm_areas;

extern mm_t* mm_current;

mm_t* mm_init();

mm_t* mm_create(ptent_t *root);

mm_t* mm_clone(mm_t *mm_root);

void mm_free(mm_t *mm);

void mm_dump_mm(mm_t* mm);

#endif /*__LWC_LIBLWC_VM_MM_H__*/









