#ifndef __LWC_LIBLWC_INC_SYSCALL_H__
#define __LWC_LIBLWC_INC_SYSCALL_H__

#define _GNU_SOURCE
#include <unistd.h>
#include <sys/syscall.h>
#include "../core/lwc_types.h"
#include "../syscalls/lwc_sysnr.h"

static inline int lwc_create(	lwc_rg_struct *mod,
								unsigned int numr,
								lwc_res_t *res)
{
	return (int) syscall(SYS_LWC_CREATE, mod, numr, res);
}
static inline int lwc_switch(lwc_struct *lwc, void *args, lwc_res_t *res)
{
	return (int) syscall(SYS_LWC_SWITCH, lwc, args, res);
}
static inline int lwc_switch_discard(lwc_struct *lwc, void *args, lwc_res_t *res)
{
	return (int) syscall(SYS_LWC_SWITCH_DISCARD, lwc, args, res);
}
static inline int lwc_println(void* arg, int flags, int id)
{
	return (int) syscall(SYS_LWC_PRINT, arg, flags, id);
}
#endif /*__LWC_LIBLWC_INC_SYSCALL_H__*/