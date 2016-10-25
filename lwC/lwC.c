#include <pthread.h>

#include "lwC.h"
#include "lwc_vm.h"
#include "lwc_errno.h"


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
/*typedef long(*bsysfn_t)(uint64_t, uint64_t, uint64_t, uint64_t);
static bsysfn_t bsys_tbl[] = {
    // (bsysfn_t) bsys_udp_send,
    // (bsysfn_t) bsys_udp_sendv,
    // (bsysfn_t) bsys_udp_recv_done,
    // (bsysfn_t) bsys_tcp_connect,
    // (bsysfn_t) bsys_tcp_accept,
    // (bsysfn_t) bsys_tcp_reject,
    // (bsysfn_t) bsys_tcp_send,
    // (bsysfn_t) bsys_tcp_sendv,
    // (bsysfn_t) bsys_tcp_recv_done,
    // (bsysfn_t) bsys_tcp_close,
};*/

typedef uint64_t (*sysfn_t)(uint64_t, uint64_t, uint64_t,
                uint64_t, uint64_t, uint64_t);
static sysfn_t sys_tbl[] = {
    // (sysfn_t) sys_bpoll,
    // (sysfn_t) sys_bcall,
    // (sysfn_t) sys_baddr,
    // (sysfn_t) sys_mmap,
    // (sysfn_t) sys_unmap,
    // (sysfn_t) sys_spawnmode,
    // (sysfn_t) sys_nrcpus,
    // (sysfn_t) sys_timer_init,
    // (sysfn_t) sys_timer_ctl,
};

struct spawn_req {
    void *arg;
    struct spawn_req *next;
};
static struct spawn_req *spawn_reqs;

//Original defined in dp/core/syscall.
bool sys_spawn_cores;
static pthread_mutex_t spawn_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t spawn_cond = PTHREAD_COND_INITIALIZER;

//Original defined in dp/core/syscall.c
void do_syscall(struct dune_tf *tf, uint64_t sysnr)
{   
    //TODO problem cannot find SYS_NR
    /*if (unlikely(sysnr >= SYS_NR)) {
        tf->rax = (uint64_t) - ENOSYS;
        return;
    }*/

    // KSTATS_POP(NULL);
    tf->rax = (uint64_t) sys_tbl[sysnr](tf->rdi, tf->rsi, tf->rdx,
                        tf->rcx, tf->r8, tf->r9);
    // KSTATS_PUSH(user, NULL);
}


//Original defined in dp/core/init.c
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


