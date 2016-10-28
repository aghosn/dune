#include <stdio.h>
#include <pthread.h>

#include "lwC.h"
#include "lwc_vm.h"
/*******************************************************************************
 *                      TODO needed for sandbox.
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
 *                         Entry point.
 ******************************************************************************/
void do_syscall(struct dune_tf *tf, uint64_t sysnr) {   
    printf("In do syscall\n");
    fflush(stdout);
    // tf->rax = (uint64_t) sys_tbl[sysnr](tf->rdi, tf->rsi, tf->rdx,
                        // tf->rcx, tf->r8, tf->r9);
}

int main(int argc, char *argv[]) {
    printf("Welcome to lwc!\n");
    
    dune_init(false);
    dune_enter();
    printf("Initialized dune.\n");

    printf("Will load the sandbox now.\n");
    sandbox_init("/lib64/ld-linux-x86-64.so.2", argc - 1, &argv[1]);

    printf("Sandbox finished execution.\n");
    return 0;
}





