#ifndef __LWC_LIBLWC_INC_SYSCALL_H__
#define __LWC_LIBLWC_INC_SYSCALL_H__

#define _GNU_SOURCE
#include <unistd.h>
#include <sys/syscall.h>
#include "../core/lwc_types.h"
#include "../syscalls/lwc_sysnr.h"

static inline int lwc_create(lwc_rsrc_spec *mod, lwc_res_t *res)
{
	return (int) syscall(SYS_LWC_CREATE, mod, res);
}
static inline int lwc_switch(lwc_struct *lwc, void *args)
{
	return (int) syscall(SYS_LWC_SWITCH, lwc, args);
}

static inline int lwc_fake_create()
{
	return (int) syscall(SYS_FAKE_CREATE);
}

static inline int lwc_fake_switch()
{
	return (int) syscall(SYS_FAKE_SWITCH);
}
#endif /*__LWC_LIBLWC_INC_SYSCALL_H__*/