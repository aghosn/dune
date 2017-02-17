#include <stdio.h>
#include <stdlib.h>
#include <dune.h>
#include <sandbox/sandbox.h>
#include <mm/memory.h>

#include "lwc.h"

int main(int argc, char* argv[])
{
	int ret = 0;
    
    if ((ret = dune_init(false)))
    	return ret;

    if ((ret = dune_enter()))
    	return ret;

    if ((ret = lwc_init()))
    	return ret;

    assert(lwc_root != NULL && lwc_root->vm_mm == mm_root && mm_root != NULL &&
        mm_root->pml4 == pgroot);
    assert(mm_root == memory_get_mm());
    
#ifdef DEBUG
    mm_verify_mappings(lwc_root->vm_mm);
#endif

    /* Entering the sandbox.*/
    //TODO: sanitize arguments.
    uintptr_t sp, entry;
    if ((ret = sandbox_init(
    	"/lib64/ld-linux-x86-64.so.2", argc-1, &argv[1], &sp, &entry))) {
    	return ret;
    }

#ifdef DEBUG
    mm_verify_mappings(lwc_root->vm_mm);
#endif
    ret = sandbox_run_app(sp, entry);
    
    return ret;
}