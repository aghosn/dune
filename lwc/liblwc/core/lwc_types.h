#ifndef __LWC_CORE_LWC_TYPES_H__
#define __LWC_CORE_LWC_TYPES_H__

#include <dune.h>
#include <utils/vq.h>
#include <mm/mm_types.h>

#define D_NORMA 0x0001
#define D_DEREF 0x0002
#define D_MMMAP	0x0008
#define D_TRAPF 0x0010

/* Defines a list of lwc_structs.*/
TAILQ_HEAD(lwc_list, lwc_struct);

typedef struct lwc_struct {
	mm_struct *vm_mm;
	struct dune_tf tf;
	
	struct lwc_struct *parent;
	struct lwc_list children;

	/* For management*/
	TAILQ_ENTRY(lwc_struct) q_ctx;
	TAILQ_ENTRY(lwc_struct) q_parent;
	
} lwc_struct;

/* Modifier types.*/
typedef enum lwc_rgopt_e {
	LWC_COW = 1,
	LWC_SHARED = 2,
	LWC_UNMAP = 3,
	LWC_RO = 4
} lwc_rgopt_e;

/* Descriptor for a range operator.*/
typedef struct lwc_rg_struct {
	vm_addrptr start;
	vm_addrptr end;
	lwc_rgopt_e opt;
} lwc_rg_struct;

/* The result type for lwc system calls*/
typedef struct lwc_res_t {
	lwc_struct *n_lwc;
	lwc_struct *caller;

	/* Extra arguments.*/
	void *args;
} lwc_res_t;

#endif /*__LWC_CORE_LWC_TYPES_H__*/