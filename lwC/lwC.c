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

void do_syscall(struct dune_tf *tf, uint64_t sysnr) {
    printf("In do syscall\n");
    fflush(stdout);
}
