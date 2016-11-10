#ifndef __LWC_CORE_LWC_TYPES_H__
#define __LWC_CORE_LWC_TYPES_H__

#include <dune.h>
#include <utils/vq.h>
#include <mm/mm_types.h>

/* Defines a list of lwc_structs.*/
Q_NEW_HEAD(l_lwc, lwc_struct); 

typedef struct lwc_struct {
	mm_struct *vm_mm;
	struct dune_tf tf;
	
	struct lwc_struct *parent;
	l_lwc children;

	/* For management*/
	Q_NEW_LINK(lwc_struct) lk_ctx;
	Q_NEW_LINK(lwc_struct) lk_parent;
} lwc_struct;

/* Modifier types.*/
typedef enum lwc_rgopt_e {
	LWC_COW = 1,
	LWC_SHARED = 2,
	LWC_UNMAP = 3
} lwc_rgopt_e;

/* Descriptor for a range operator.*/
typedef struct lwc_rg_struct {
	vm_addrptr start;
	vm_addrptr end;
	lwc_rgopt_e opt;

	Q_NEW_LINK(lwc_rg_struct) lk_rg;
} lwc_rg_struct;

/* List of lwc_rg_struct.*/
Q_NEW_HEAD(l_lwc_rg, lwc_rg_struct);

/* Specifies the modifications to perform.*/
typedef struct lwc_rsrc_spec {
	l_lwc_rg ranges;

	//TODO: add other information as need.
} lwc_rsrc_spec;

#endif /*__LWC_CORE_LWC_TYPES_H__*/