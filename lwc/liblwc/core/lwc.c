#include <stdio.h>
#include <pthread.h>
#include <sandbox/sandbox.h>

#include <dune.h>

#include "lwc.h"
#include "lwc_vm.h"
#include <mm/memory.h>
/*******************************************************************************
 *                      Needed for sandbox.
 *                      TODO: move it somewhere else.
*******************************************************************************/
bool sys_spawn_cores;

static pthread_mutex_t spawn_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t spawn_cond = PTHREAD_COND_INITIALIZER;

struct spawn_req {
    void *arg;
    struct spawn_req *next;
};

static struct spawn_req *spawn_reqs;

int init_do_spawn(void *arg) {
    struct spawn_req *req;

    pthread_mutex_lock(&spawn_mutex);
    req = malloc(sizeof(struct spawn_req));
    if (!req) {
        pthread_mutex_unlock(&spawn_mutex);
        return -ENOMEM;
    }

    req->next = spawn_reqs;
    req->arg = arg;
    spawn_reqs = req;
    pthread_cond_broadcast(&spawn_cond);
    pthread_mutex_unlock(&spawn_mutex);

    return 0;
    
}
/******************************************************************************/
/**
 * The root light-context. Initialized by lwc_init();
 */
lwc_context_t __lwc_root;

void lwc_init(int argc, char *argv[]) {
    //Init of the sandbox but don't run it just yet.
    uintptr_t sp, entry;
    
    sandbox_init("/lib64/ld-linux-x86-64.so.2", argc, argv, &sp, &entry);
    sandbox_run_app(sp, entry);
    //mm_dump(mm_root);
    //dune_procmap_dump();
}

//TODO place holders for an ASM function
// static int lwc_fork(){return 0;}
// static lwc_context_t* __lwc_get_current() {return NULL;}

lwc_result_t lwc_create(lwc_resource_spec_t specs, uint64_t options) {
    //Copy memory
    //should trap
    //TODO copy credentials
    //TODO copy syscall
    //TODO get filedescriptor.
    
    //TODO this is prototype code.
    //Assume we have the parent
    //lwc_context_t* parent_ctx = __lwc_get_current();
    
    // int caller_id = 9;
    
    //Result for the child.
    // lwc_result_t child_res = {parent, caller_id, NULL}; //TODO do this.
    
    //lwc created.
    // lwc_context_t *lwc_ctx = malloc(sizeof(lwc_context_t)); //TODO or kmalloc
    
    // lwc_ctx->pml4 = lwc_cow_copy_pgroot(parent_ctx->pml4);

    // lwc_result_t parent_res = {lwc_child, -1, NULL}; 

    //TODO need separate stack for this part
    //New stack will have all the regs pushed on it
    //TODO init the lwc_child.
    // ...
    /*This is where we return from the child, rax has to point to proper result*/
    // int res = lwc_fork();
    // if (res == -1)
        // goto FROM_CHILD;

    // return parent_res;

// FROM_CHILD:
    lwc_result_t t;
    return t;
}

lwc_result_t lwc_switch(lwc_context_t l, void* args) {
	//TODO implement. Need to load vm, and registers.
	// struct dune_tf tf;

    // tf.rip = l.rip;
    // tf.rsp = l.rsp;
    // tf.rflags = 0x0;

    // int ret = dune_jump_to_user(&tf);
    //TODO just to avoid warning.
    // printf("The return value %d\n", ret);
    //TODO use the trap frame to return the correct result.
	lwc_result_t t;
	return t;
}



/*******************************************************************************
 *                         Entry point.
 ******************************************************************************/
void do_syscall(struct dune_tf *tf, uint64_t sysnr) {   
    printf("In do syscall\n");
    fflush(stdout);
}

int main(int argc, char *argv[]) {
    printf("Welcome to lwc!\n");
    
    dune_init(false);
    dune_enter();
    printf("Initialized dune.\n");

    printf("Initialize the sandbox and lwc root.\n");
    lwc_init(argc-1, &argv[1]);
    
    //sandbox_init_run("/lib64/ld-linux-x86-64.so.2", argc - 1, &argv[1]);

    printf("Sandbox finished execution.\n");
    return 0;
}





