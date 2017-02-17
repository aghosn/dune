#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <pthread.h>
#include <stdbool.h>

#include <dune.h>
#include <mm/memory.h>
#include <mm/vm_tools.h>
#include <mm/mm_tools.h>
#include <sandbox/sandbox.h>

#include "lwc.h"
#include "lwc_mm.h"
#include "lwc_types.h"

/* Global variables for lwc.*/

lwc_struct *lwc_root = NULL;

struct lwc_list *ctxts = NULL;

int lwc_init()
{
    int ret = 0;
    /* Allocate and init global variables.*/
    lwc_root = malloc(sizeof(lwc_struct));
    if (!lwc_root) {
        ret = -ENOMEM;
        goto err;
    }

    Q_INIT_ELEM(lwc_root, lk_parent);
    Q_INIT_HEAD(&(lwc_root->children));
    ASSERT_DBG(mm_root != NULL, "mm_root is null.\n");

    lwc_root->vm_mm = mm_root;
    lwc_root->parent = NULL;

    /*TODO new*/
    ctxts = malloc(sizeof(struct lwc_list));
    if (!ctxts) {
        ret = -ENOMEM;
        goto err;
    }

    TAILQ_INIT(ctxts);
    TAILQ_INSERT_HEAD(ctxts, lwc_root, q_ctx);

    return 0;
err:
    if (lwc_root)
        free(lwc_root);
    if (ctxts)
        free(ctxts);
    return ret;
}


int sys_lwc_create( struct dune_tf *tf,
                    lwc_rg_struct *mod,
                    unsigned int numr,
                    lwc_res_t *res)
{
    mm_struct *copy = NULL;
    lwc_struct *n_lwc = NULL, *current = NULL;

    /* Return values require res to be allocated.*/
    if (!res) {
        printf("No res! %d \n", numr);
        return -1;
    }
    
    current = TAILQ_FIRST(ctxts);
    if (!current) {
        printf("Current is null.\n");
        goto err;
    }

    mm_verify_mappings(current->vm_mm);

    /* Set the result for the child.*/
    res->n_lwc = NULL;
    res->caller = current;
    res->args = NULL;

    /* Default copy-on-write behaviour.*/
    if (!mod || numr == 0) {
        copy = mm_copy(current->vm_mm, true, true);
        goto create;
    }

    /* Slower copy.*/
    copy = lwc_mm_create(current->vm_mm, mod, numr);
    if (!copy) goto err;
    
create:
    n_lwc = malloc(sizeof(lwc_struct));
    if (!n_lwc)
        goto err;
    n_lwc->vm_mm = copy;
    n_lwc->parent = current;
    Q_INIT_HEAD(&(n_lwc->children));

    /* Initialize the dune trap frame.*/
    n_lwc->tf = *tf;
    
    /* Create the child result, might fault.*/
    res->n_lwc = n_lwc;
    res->caller = NULL;
    res->args = NULL;

    /* Child store the result address for now.*/
    n_lwc->tf.rax = (uint64_t) res;

    Q_INIT_ELEM(n_lwc, lk_parent);
    Q_INSERT_TAIL(&(current->children), n_lwc, lk_parent);
    
    TAILQ_INSERT_TAIL(ctxts, n_lwc, q_ctx);

    Q_INIT_ELEM(n_lwc->vm_mm, lk_mms);
    Q_INSERT_TAIL(mm_queue, n_lwc->vm_mm, lk_mms);

    return 1;
err:
    if (n_lwc)
        lwc_free(n_lwc);
    return -1;
}

int sys_lwc_switch( struct dune_tf *tf,
                    lwc_struct *lwc,
                    void *args,
                    lwc_res_t *res)
{
    ASSERT_DBG(tf, "trap frame is null.\n");
    ASSERT_DBG(lwc, "lwc is null.\n");

    /* Get the current context.*/
    lwc_struct *current = TAILQ_FIRST(ctxts);
    
    /* Sanity checks.*/
    ASSERT_DBG(current, "current is null.\n");
    ASSERT_DBG(current->vm_mm, "current->vm_mm is null.\n");
    mm_struct * __current_mm = Q_GET_FRONT(mm_queue);
    ASSERT_DBG(current->vm_mm == __current_mm,
        "current->vm_mm{0x%016lx}, __current_mm{0x%016lx}\n",
        (unsigned long)current->vm_mm,(unsigned long)__current_mm); 
    ASSERT_DBG(__current_mm->pml4 == pgroot,
        "__current_mm->pml4{0x%016lx}, pgroot{0x%016lx}\n",
        (unsigned long)__current_mm->pml4, (unsigned long)pgroot);

    TAILQ_REMOVE(ctxts, lwc, q_ctx);
    TAILQ_INSERT_HEAD(ctxts, lwc, q_ctx);

    /* Save the current point.*/
    current->tf = *tf;

    /* Set the pointer to the result for when we switch back.*/
    current->tf.rax = (uint64_t) res;

    /* Do the switch*/
    *tf = lwc->tf;
    memory_switch(lwc->vm_mm);
    lwc_res_t *target_res = (lwc_res_t*) (lwc->tf.rax);

    if (!target_res) {
        return -1;
    }
    target_res->n_lwc = NULL;
    target_res->caller = current;
    target_res->args = args;

    /* Set return value.*/
    lwc->tf.rax = 0;
    tf->rax = 0;
    
    return 0;
}

int sys_fake_println(struct dune_tf *tf, void* arg, int flags, int id)
{
    printf("--------------------------------------------------------------------------------\n");
    if (flags & D_NORMA) {
        printf("%d__(NORMA) argument address: 0x%016lx\n", id, (unsigned long)arg);
    }

    if (flags & D_DEREF) {
        printf("%d__(DEREF) deref element: 0x%016lx\n", id,
            (unsigned long) *((char *)arg));
    }

    if (flags & D_MMMAP) {
        printf("%d__(MMAP) dumping the memory mappings.\n", id);
        mm_struct* current = memory_get_mm();
        ASSERT_DBG(current, "current is null.\n");
        mm_dump(current);
    }

    if (flags & D_TRAPF) {
        printf("%d__(TRAPF) dumping the trap frame.\n", id);
        dune_dump_trap_frame(tf);
    }
    printf("\n");
    fflush(stdout);
    return 0;
}

int lwc_free(lwc_struct *lwc)
{
    ASSERT_DBG(lwc, "lwc is null.\n");
    if (lwc->parent) {
        lwc_struct *parent = lwc->parent;
        Q_REMOVE(&(parent->children), lwc, lk_parent);
    }

    TAILQ_REMOVE(ctxts, lwc, q_ctx);

    mm_free(lwc->vm_mm);
    free(lwc);
    return 0;
}