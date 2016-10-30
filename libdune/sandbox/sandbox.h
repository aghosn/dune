/*
 * Copyright 2013-16 Board of Trustees of Stanford University
 * Copyright 2013-16 Ecole Polytechnique Federale Lausanne (EPFL)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

/*
 * sandbox.h - the main local header
 */

#ifndef __LIBDUNE_SANDBOX_H__
#define __LIBDUNE_SANDBOX_H__

#include <errno.h>
#include <sys/types.h>
#include <stdint.h>

//#include "mem.h"
#include "../dune.h"

//TODO aghosn: not copied from anywhere, replacing kstats.h
#define log_err(fmt, ...) printf(fmt, ##__VA_ARGS__)
#define log_info(fmt, ...) printf(fmt, ##__VA_ARGS__)
#define log_debug(fmt, ...) printf(fmt, ##__VA_ARGS__)

//TODO aghosn: replacing KSTATS
#define KSTATS_PUSH(a, b)	
#define KSTATS_POP(a)

#define LOADER_VADDR_OFF	0x6F000000
#define APP_STACK_SIZE		0x800000 /* 8 megabytes */

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
extern int MEMORY_BASE_ADDR;

int sandbox_init_run(char *loader, int argc, char *argv[]);

int sandbox_init(char *loader, int argc, char *argv[], 
											uintptr_t *sp, uintptr_t *entry);
int run_app(uintptr_t sp, uintptr_t e_entry);
#endif /* __LIBDUNE_SANDBOX_H__ */

