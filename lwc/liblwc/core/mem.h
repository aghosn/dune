#ifndef __LIBDUNE_MEM_H__
#define __LIBDUNE_MEM_H__

#define MEM_SANDBOX_BASE_ADDR	0x70000000   /* the sandbox ELF is loaded here */
#define MEM_PHYS_BASE_ADDR		0x4000000000 /* memory is allocated here (2MB going up, 1GB going down) */
#define MEM_USER_DIRECT_BASE_ADDR	0x7000000000 /* start of direct user mappings (P = V) */
#define MEM_USER_DIRECT_END_ADDR	0x7F00000000 /* end of direct user mappings (P = V) */

#endif /*__LIBDUNE_MEM_H__*/
