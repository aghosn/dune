#include <stdio.h>
#include <stdlib.h>
#include "libdune/dune.h"
#include "libdune/mm/memory.h"


void heap_alloc_pgflt_handler(uintptr_t addr, uint64_t fec, struct dune_tf *tf)
{
	if ((fec & FEC_U))
		memory_pgflt_handler(addr, fec, tf);

	printf("Page fault: 0x%016lx ----> 0x%016lx\n", addr, fec);
	ptent_t *pte = NULL;
	int ret = dune_vm_lookup(pgroot, (void *) addr, CREATE_NORMAL, &pte);
	assert(!ret);
}


int main(int argc, char* argv[])
{
	int ret = 0;

	if ((ret = dune_init(false)))
		return ret;

	if ((ret = dune_enter()))
		return ret;

	dune_register_pgflt_handler(heap_alloc_pgflt_handler);

	for (int i = 0; i < 25; i++) {
		printf("Allocating.\n");
		void* ptr = memalign(PGSIZE, PGSIZE);
	}

	return 0;
}