#ifndef __LWC_LIBLWC_INC_SYSCALL_H__
#define __LWC_LIBLWC_INC_SYSCALL_H__

#define _GNU_SOURCE
#include <unistd.h>
#include <sys/syscall.h>
#include "../core/lwc_types.h"
#include "../syscalls/syscall_ids.h"

static inline lwc_struct* lwc_create(lwc_rsrc_spec *mod)
{
	return (lwc_struct*) syscall(SYS_LWC_CREATE, mod);
}
static inline int lwc_switch(lwc_struct *lwc, void *args)
{
	return (int) syscall(SYS_LWC_SWITCH, args);
}
#endif /*__LWC_LIBLWC_INC_SYSCALL_H__*/