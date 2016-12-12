#ifndef __LWC_CORE_LWC_H__
#define __LWC_CORE_LWC_H__

#include "lwc_types.h"

/* Global variables.*/
extern lwc_struct *lwc_root;
extern l_lwc *contexts;

/* lwc API*/

int lwc_init();

int sys_lwc_create(struct dune_tf *tf, lwc_rsrc_spec *mod, lwc_res_t *res);

int sys_lwc_switch(	struct dune_tf *tf, 
					lwc_struct *lwc,
					void *args,
					lwc_res_t * res);

int sys_fake_println(struct dune_tf *tf, void* arg, int flags, int id);

int lwc_free(lwc_struct *lwc);
#endif /*__LWC_CORE_LWC_H__*/