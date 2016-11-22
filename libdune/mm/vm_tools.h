#ifndef __LIBDUNE_MM_VM_TOOLS_H__
#define __LIBDUNE_MM_VM_TOOLS_H__
#include <stdlib.h>
#include <stdbool.h>

#include "../dune.h"
#include "../mmu.h"
#include "../mmu-x86.h"
#include "vm.h"

//FIXME: check that again.
#define __PPTE_FLAGS (UINT64(PTE_NX | PTE_PAT | PTE_COW | PTE_PCD | PTE_PWT | PTE_U | PTE_W | PTE_P))

#define PPTE_FLAGS(pte) ((physaddr_t) (pte) & __PPTE_FLAGS)

typedef struct cb_info {
	int level;
	void *args;
} cb_info;


typedef int (*pgrot_walk_cb) (ptent_t* pte, void *va, cb_info* args);

int vm_pgrot_walk(	ptent_t *root,
					void *start,
					void *end,
					pgrot_walk_cb cb,
					pgrot_walk_cb alloc,
					void *args);

int vm_create_phys_mapping(ptent_t *root, void *start, void *end, int perm);

/* Looks up a given address inside the page tables.
 * Enables to create the intermediary mappings if they do not exist.
 * The provided flags are used while creating inter-level entries.
 * The final entry is not modified, but returned via pte_out.
 * The value create enables to select the creation of big mappings
 * Note: the last functionality was missing in dune_vm_lookup.*/
int vm_lookup(	ptent_t* root,
				void *va,
				ptent_t** pte_out,
				int create,
				ptent_t flags);

ptent_t* vm_pgrot_copy(ptent_t* root, bool cow);

int vm_uncow(ptent_t* root, void *addr);

int vm_compare_pgroots(ptent_t* o, ptent_t *c);
#endif /*__LIBDUNE_MM_VM_TOOLS_H__*/