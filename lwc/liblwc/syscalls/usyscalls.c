#include <stdio.h>
#include <pthread.h>

#include <dune.h>
#include <sandbox/sandbox.h>

/* Implementation required for the sandbox.*/
bool sys_spawn_cores;

static pthread_mutex_t spawn_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t spawn_cond = PTHREAD_COND_INITIALIZER;

struct spawn_req {
    void *arg;
    struct spawn_req *next;
};

static struct spawn_req *spawn_reqs;

int init_do_spawn(void *arg)
{
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

//TODO: support the lwc interface.
void do_syscall(struct dune_tf *tf, uint64_t sysnr)
{   
    printf("In do syscall with id %lu\n", sysnr);
    fflush(stdout);
}