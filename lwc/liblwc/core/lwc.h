#ifndef __LWC_CORE_LWC_H__
#define __LWC_CORE_LWC_H__

#include "lwc_types.h"

/* Global variables.*/
extern lwc_struct *lwc_root;
extern l_lwc *contexts;

extern struct lwc_list *ctxts;

/* lwc API*/

/* Initialize lwc data structures and the root context.*/
int lwc_init();

/* Create an lwc according to the specified modifiers.*/
int sys_lwc_create(	struct dune_tf *tf,
					lwc_rg_struct *mod,
					unsigned int numr,
					lwc_res_t *res);

/* Switch between current context and target lwc.*/
int sys_lwc_switch(	struct dune_tf *tf, 
					lwc_struct *lwc,
					void *args,
					lwc_res_t * res);

/* TODO: remove, for debugging purposes.*/
int sys_fake_println(struct dune_tf *tf, void* arg, int flags, int id);

/* Delete an lwc and free the associated resources.*/
int lwc_free(lwc_struct *lwc);
#endif /*__LWC_CORE_LWC_H__*/