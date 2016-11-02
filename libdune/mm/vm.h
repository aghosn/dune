#ifndef __LIBDUNE_MM_VM_H__
#define __LIBDUNE_MM_VM_H__
#include <stdlib.h>

#include "../dune.h"
#include "../mmu.h"
#include "../mmu-x86.h"

#define PDADDR(n, i)	(((unsigned long)(i)) << PDSHIFT(n))
#define PTE_DEF_FLAGS	(PTE_P | PTE_W | PTE_U)
#define PGSIZE_2MB		(1 << (PGSHIFT + NPTBITS))
#define PGSIZE_1GB		(1 << (PGSHIFT + NPTBITS + NPTBITS))

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
int dune_map_phys(ptent_t *root, void *va, size_t len, void *pa, int perm);
int dune_vm_map_pages(ptent_t* root, void *va, size_t len, int perm);
ptent_t *dune_vm_clone(ptent_t *root);
void dune_vm_free(ptent_t *root);
void dune_vm_unmap(ptent_t *root, void *va, size_t len);
void dune_vm_default_pgflt_handler(uintptr_t addr, uint64_t fec);
#endif /*__LIBDUNE_MM_VM_H__*/