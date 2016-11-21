#include <stdio.h>

#include <dune.h>
#include <sandbox/sandbox.h>

#include "lwc.h"

int main(int argc, char* argv[])
{
	int ret = 0;
    printf("Welcome to lwc!\n");
    //TODO: improve error handling.
    if ((ret = dune_init(false)))
    	return ret;

    if ((ret = dune_enter()))
    	return ret;

    printf("Dune: initialized\n");

    if ((ret = lwc_init()))
    	return ret;
    printf("Lwc: initialized\n");

    /* Entering the sandbox.*/
    //TODO: sanitize arguments.
    uintptr_t sp, entry;
    if ((ret = sandbox_init(
    	"/lib64/ld-linux-x86-64.so.2", argc-1, &argv[1], &sp, &entry))) {
    	return ret;
    }

    ret = sandbox_run_app(sp, entry);
    
    printf("Lwc: execution completed. Goodbye!\n");
    return ret;
}