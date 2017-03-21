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

typedef enum copy_type {
	CB_COW = 1,
	CB_SHARE = 2,
	CB_RO = 3
} copy_type;

typedef struct cb_info {
	int level;
	void *args;
} cb_info;

struct copy_info {
	ptent_t* o_root;
	ptent_t* n_root;
	bool cow;
	copy_type type;
};

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

ptent_t *vm_pgrot_copy_range (	ptent_t *original,
				ptent_t *copy,
				void *start,
				void* end,
				bool cow,
				copy_type modifier);

int vm_uncow(ptent_t* root, void *addr);

int vm_compare_pgroots(ptent_t* o, ptent_t *c);

int vm_check_references(ptent_t* pml4, uint64_t expect_pte, uint64_t expect_o, uint64_t read_only);

/*TODO for debugging remove afterwards.*/
static inline void watchpoint_add(int idx, void *addr)
{
        unsigned long dr7;

        switch (idx) {
        case 0:
                asm("mov %0, %%dr0" : : "r" (addr));
                break;
        case 1:
                asm("mov %0, %%dr1" : : "r" (addr));
                break;
        case 2:
                asm("mov %0, %%dr2" : : "r" (addr));
                break;
        case 3:
                asm("mov %0, %%dr3" : : "r" (addr));
                break;
        default:
                /* Instead of assert(0) because I can't include the header. */
                asm("ud2");
        }
        asm("mov %%dr7, %0": "=r" (dr7));
        dr7 |= 1 << (idx * 2);
        dr7 |= 1 << (idx * 4 + 16);
        dr7 |= 0 << (idx * 4 + 18);
        asm("mov %0, %%dr7": : "r" (dr7));
}

static inline void watchpoint_delete(int idx)
{
        unsigned long dr7;
        asm("mov %%dr7, %0": "=r" (dr7));
        dr7 &= ~(1 << (idx * 2));
        asm("mov %0, %%dr7": : "r" (dr7));
}

#endif /*__LIBDUNE_MM_VM_TOOLS_H__*/