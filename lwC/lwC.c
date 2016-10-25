#include "lwC.h"
#include "lwc_vm.h"



lwc_result_t lwc_create(lwc_resource_spec_t specs, uint64_t options) {
    
    //TODO copy memory
    //should trap

    //TODO copy credentials
    //TODO copy syscall
    //TODO get filedescriptor.
    
    lwc_result_t t;
    return t;
}

lwc_result_t lwc_switch(lwc_context_t l, void* args) {
	//TODO implement. Need to load vm, and registers.
	
	lwc_result_t t;
	return t;
}

/*******************************************************************************
 *                      TODO Required for the syscalls.
*******************************************************************************/
//Original defined in dp/core/syscall.
bool sys_spawn_cores;

//Original defined in dp/core/syscall.c
void do_syscall(struct dune_tf *tf, uint64_t sysnr) {

}


//Original defined in dp/core/init.c
int init_do_spawn(void *args) {
    //TODO
    return 0;
}


