#ifndef __LIBDUNE_MM_VM_H__
#define __LIBDUNE_MM_VM_H__

#include "../dune.h"
#include "../mmu.h"
#include "../mmu-x86.h"

#define PDADDR(n, i)	(((unsigned long)(i)) << PDSHIFT(n))
//TODO: bad flags.
#define PTE_DEF_FLAGS	(PTE_P | PTE_W | PTE_U)
#define PGSIZE_2MB		(1 << (PGSHIFT + NPTBITS))
#define PGSIZE_1GB		(1 << (PGSHIFT + NPTBITS + NPTBITS))

#define UL(i) ((uint64_t) i)
#define RPDX(i, j, k, l) \
(uintptr_t)((UL(i) << PDSHIFT(3)) | (UL(j) << PDSHIFT(2)) \
	| (UL(k) << PDSHIFT(1)) | (UL(l) << PDSHIFT(0)))

typedef int (*page_walk_cb)(const void *arg, ptent_t *ptep, void *va);

static inline int pte_big(ptent_t pte) {
	return (PTE_FLAGS(pte) & PTE_PS);
}
static inline int pte_present(ptent_t pte) {
	return (PTE_FLAGS(pte) & PTE_P);
}
ptent_t get_pte_perm(int perm);

/*Operations on pages private to the vm.*/
void* alloc_page(void);
void put_page(void* page);

int dune_vm_page_walk(	ptent_t *root,
						void *start_va,
						void *end_va,
						page_walk_cb cb,
						const void *arg);
int dune_vm_lookup(ptent_t *root, void *va, int create, ptent_t **pte_out);
int dune_vm_mprotect(ptent_t *root, void *va, size_t len, int perm);
int dune_vm_map_phys(ptent_t *root, void *va, size_t len, void *pa, int perm);
int dune_vm_map_pages(ptent_t* root, void *va, size_t len, int perm);
ptent_t *dune_vm_clone(ptent_t *root);
void dune_vm_free(ptent_t *root);
void dune_vm_unmap(ptent_t *root, void *va, size_t len);
void dune_vm_default_pgflt_handler(uintptr_t addr, uint64_t fec);
int dune_vm_has_mapping(ptent_t *root, void *va);
int vm_compare_mappings(ptent_t *first, ptent_t *second);


//TODO: remove, for debug.
int vm_make_root(void* va_start, void* va_end, ptent_t* root);
int vm_count_entries(void* va_start, void* va_end, ptent_t *root);
int vm_check_entry(void *start, void *end, ptent_t* root, unsigned long flags);
#endif /*__LIBDUNE_MM_VM_H__*/