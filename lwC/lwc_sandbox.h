#ifndef __LWC_SANDBOX_H__
#define __LWC_SANDBOX_H__

#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <stdint.h>
#include <dune.h>

#define log_err(fmt, ...) printf(fmt, ##__VA_ARGS__)
#define log_info(fmt, ...) printf(fmt, ##__VA_ARGS__)
#define log_debug(fmt, ...) printf(fmt, ##__VA_ARGS__)

#define LOADER_VADDR_OFF	0x6F000000
#define APP_STACK_SIZE		0x800000 /* 8 megabytes */


//TODO change this at some point + might fail right now.
#define MEM_IX_BASE_ADDR		0x70000000   /* the IX ELF is loaded here */
#define MEM_PHYS_BASE_ADDR		0x4000000000 /* memory is allocated here (2MB going up, 1GB going down) */
#define MEM_USER_DIRECT_BASE_ADDR	0x7000000000 /* start of direct user mappings (P = V) */
#define MEM_USER_DIRECT_END_ADDR	0x7F00000000 /* end of direct user mappings (P = V) */
#define MEM_USER_IOMAPM_BASE_ADDR	0x8000000000 /* user mappings controlled by IX */
#define MEM_USER_IOMAPM_END_ADDR	0x100000000000 /* end of user mappings controlled by IX */
#define MEM_USER_IOMAPK_BASE_ADDR	0x100000000000 /* batched system calls and network mbuf's */
#define MEM_USER_IOMAPK_END_ADDR	0x101000000000 /* end of batched system calls and network mbuf's */

#define MEM_USER_START			MEM_USER_DIRECT_BASE_ADDR
#define MEM_USER_END			MEM_USER_IOMAPM_END_ADDR

#define MEM_ZC_USER_START		MEM_USER_IOMAPM_BASE_ADDR
#define MEM_ZC_USER_END			MEM_USER_IOMAPK_END_ADDR

#ifndef MAP_FAILED
#define MAP_FAILED	((void *) -1)
#endif

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
	if (begin < MEM_IX_BASE_ADDR && end < MEM_IX_BASE_ADDR)
		return true;

	/* allow the user direct memory area */
	if (begin >= MEM_USER_DIRECT_BASE_ADDR &&
	    end <= MEM_USER_DIRECT_END_ADDR)
		return true;

	/* default deny everything else */
	return false;
}

extern int check_extent(const void *ptr, size_t len);
extern int check_string(const void *ptr);

static inline long get_err(long ret)
{
	if (ret < 0)
		return -errno;
	else
		return ret;
}

extern int elf_load(const char *path);

extern unsigned long umm_brk(unsigned long brk);
extern unsigned long umm_mmap(void *addr, size_t length, int prot, int flags,
			      int fd, off_t offset);
extern int umm_munmap(void *addr, size_t len);
extern int umm_mprotect(void *addr, size_t len, unsigned long prot);
extern void *umm_shmat(int shmid, void *addr, int shmflg);
extern int umm_alloc_stack(uintptr_t *stack_top);
extern void *umm_mremap(void *old_address, size_t old_size,
			size_t new_size, int flags, void *new_address);

extern int trap_init(void);


#endif