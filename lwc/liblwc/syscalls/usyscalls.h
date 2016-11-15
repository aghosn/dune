#ifndef __LWC_LIBLWC_SYSCALLS_USYSCALLS_H__
#define __LWC_LIBLWC_SYSCALLS_USYSCALLS_H__

#include <dune.h>

typedef long (*lwc_sysfn_t) (struct dune_tf*, uint64_t, uint64_t, uint64_t,
							uint64_t, uint64_t, uint64_t);
#endif /*__LWC_LIBLWC_SYSCALLS_USYSCALLS_H__*/