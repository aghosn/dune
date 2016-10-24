#ifndef __LWC_VM_H__
#define __LWC_VM_H__

#include <dune.h>
#include <sys/types.h>


/*VM helpers*/
#define PTE_DEF_FLAGS	(PTE_P | PTE_W | PTE_U)
#define PTE_MAKE_COW(pte) (((pte) & (~PTE_W)) | PTE_COW)
#define PDADDR(n, i)	(((unsigned long) (i)) << PDSHIFT(n))

#define LWC_CREATE_NONE 0
#define LWC_CREATE_NORMAL 1
#define LWC_CREATE_BIG 2
#define LWC_CREATE_BIG_1GB 4

typedef int (*lwc_page_cb)(const void *arg, ptent_t *ptep, void *va, int level);

struct copy_root_t {
	ptent_t *original;
	ptent_t *copy;
};

/*Helper functions*/
ptent_t* lwc_cow_copy_pgroot(ptent_t* root);
ptent_t* lwc_copy_pgroot(ptent_t* root);

#endif