#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

#include "../dune.h"
#include "vm.h"

ptent_t get_pte_perm(int perm) {
	ptent_t pte_perm = 0;

	if (perm & PERM_R)
		pte_perm = PTE_P;
	if (perm & PERM_W)
		pte_perm |= PTE_W;
	if (!(perm & PERM_X))
		pte_perm |= PTE_NX;
	if (perm & PERM_U)
		pte_perm |= PTE_U;
	if (perm & PERM_UC)
		pte_perm |= PTE_PCD;
	if (perm & PERM_COW)
		pte_perm |= PTE_COW;
	if (perm & PERM_USR1)
		pte_perm |= PTE_USR1;
	if (perm & PERM_USR2)
		pte_perm |= PTE_USR2;
	if (perm & PERM_USR3)
		pte_perm |= PTE_USR3;
	if (perm & PERM_BIG || perm & PERM_BIG_1GB)
		pte_perm |= PTE_PS;

	return pte_perm;
}

