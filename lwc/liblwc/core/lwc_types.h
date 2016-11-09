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
	l_lwc *children;

	/* For management*/
	Q_NEW_LINK(lwc_struct) lk_ctx;
	Q_NEW_LINK(lwc_struct) lk_parent;
} lwc_struct;


#endif /*__LWC_CORE_LWC_TYPES_H__*/