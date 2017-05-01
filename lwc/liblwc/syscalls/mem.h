#ifndef __LIBDUNE_MEM_H__
#define __LIBDUNE_MEM_H__

#define MEM_SANDBOX_BASE_ADDR	0x70000000   /* the sandbox ELF is loaded here */
#define MEM_PHYS_BASE_ADDR		0x4000000000 /* memory is allocated here (2MB going up, 1GB going down) */
#define MEM_USER_DIRECT_BASE_ADDR	0x7000000000 /* start of direct user mappings (P = V) */
#define MEM_USER_DIRECT_END_ADDR	0x7F00000000 /* end of direct user mappings (P = V) */

/**
 * mem_ref_is_safe - determines if a memory range belongs to the sandboxed app
 * @ptr: the base address
 * @len: the length
 */
static inline bool mem_ref_is_safe(const void *ptr, size_t len)
{
	uintptr_t begin = (uintptr_t) ptr;
	uintptr_t end = (uintptr_t)(ptr + len);

	/* limit possible overflows */
	if (len > MEM_USER_DIRECT_END_ADDR - MEM_USER_DIRECT_BASE_ADDR)
		return false;

	/* allow ELF data */
	if (begin < MEM_SANDBOX_BASE_ADDR && end < MEM_SANDBOX_BASE_ADDR)
		return true;

	/* allow the user direct memory area */
	if (begin >= MEM_USER_DIRECT_BASE_ADDR &&
	    end <= MEM_USER_DIRECT_END_ADDR)
		return true;

	/* default deny everything else */
	return false;
}
#endif /*__LIBDUNE_MEM_H__*/
