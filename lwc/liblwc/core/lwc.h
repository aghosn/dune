#ifndef __LWC_CORE_LWC_H__
#define __LWC_CORE_LWC_H__

#include "lwc_types.h"

/* Global variables.*/
extern lwc_struct *lwc_root;
extern l_lwc *contexts;

/* lwc API*/
int lwc_init();

#endif /*__LWC_CORE_LWC_H__*/