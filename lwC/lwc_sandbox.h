#ifndef __LWC_SANDBOX_H__
#define __LWC_SANDBOX_H__

#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <stdint.h>
#include <dune.h>

#define log_err(fmt, ...) printf(fmt, ##__VA_ARGS__)
#define log_info(fmt, ...) printf(fmt, ##__VA_ARGS__)

#define LOADER_VADDR_OFF	0x6F000000


//TODO change this at some point + might fail right now.
#define MEM_IX_BASE_ADDR		0x70000000   /* the IX ELF is loaded here */

struct elf_data {
	uintptr_t entry;
	Elf64_Phdr *phdr;
	int phnum;
};

#endif