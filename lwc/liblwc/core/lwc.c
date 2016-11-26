#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <pthread.h>
#include <stdbool.h>

#include <dune.h>
#include <mm/memory.h>
#include <sandbox/sandbox.h>


#include "lwc.h"
#include "lwc_types.h"

/* Global variables for lwc.*/
lwc_struct *lwc_root = NULL;
l_lwc *contexts = NULL;

int lwc_init()
{
    int ret = 0;
    /* Allocate and init global variables.*/
    lwc_root = malloc(sizeof(lwc_struct));
    if (!lwc_root) {
        ret = -ENOMEM;
        goto err;
    }

    Q_INIT_ELEM(lwc_root, lk_ctx);
    Q_INIT_ELEM(lwc_root, lk_parent);
    Q_INIT_HEAD(&(lwc_root->children));
    assert(mm_root != NULL);

    lwc_root->vm_mm = mm_root;
    lwc_root->parent = NULL;

    contexts = malloc(sizeof(l_lwc));
    if (!contexts) {
        ret = -ENOMEM;
        goto err;
    }
    Q_INIT_HEAD(contexts);
    Q_INSERT_FRONT(contexts, lwc_root, lk_ctx);

    return 0;
err:
    if (lwc_root)
        free(lwc_root);
    if (contexts)
        free(contexts);
    return ret;
}

//TODO hum for cow might need the original.
static mm_struct* lwc_apply_mm(lwc_rsrc_spec *mod, mm_struct *o)
{
    assert(mod && o);
    mm_struct *copy = mm_copy(o, false, true);
    if (!copy)
        goto err;

    lwc_rg_struct *current = NULL;
    Q_FOREACH(current, &(mod->ranges), lk_rg) {
        switch(current->opt) {
            case LWC_COW:
                //mm_cow(o, copy, current->start, current->end, false);
                break;
            case LWC_SHARED:
                //mm_shared(o, copy, current->start, current->end, false);
                break;
            case LWC_UNMAP:
                //mm_unmap(copy, current->start, current->end, false);
                break;
            default:
                //TODO: logg error.
                goto err;
        }
    }
    //FIXME: do the apply.
    mm_apply(o);
    mm_apply(copy);
    return copy;

err:
    return NULL;
}

int sys_lwc_create(struct dune_tf *tf, lwc_rsrc_spec *mod, lwc_res_t *res)
{
    mm_struct *copy = NULL;
    lwc_struct *n_lwc = NULL, *current = NULL;

    /* Error.*/
    if (!res)
        return -1;
    

    /* The current context.*/
    current = Q_GET_FRONT(contexts);
    if (!current)
        goto err;

    //TODO: remove, for debugging.
    mm_verify_mappings(current->vm_mm);

    /* Set the result for the child.*/
    res->n_lwc = NULL;
    res->caller = current;
    res->args = NULL;

    /* Default copy-on-write behaviour.*/
    if (!mod || mod->ranges.head == NULL) {
        copy = mm_copy(current->vm_mm, true, true);
        goto create;
    }

    /* Slower copy.*/
    copy = lwc_apply_mm(mod, current->vm_mm);

create:
    n_lwc = malloc(sizeof(lwc_struct));
    if (!n_lwc)
        goto err;
    n_lwc->vm_mm = copy;
    n_lwc->parent = current;
    Q_INIT_HEAD(&(n_lwc->children));

    /* Initialize the dune trap frame.*/
    memcpy(&(n_lwc->tf), tf, sizeof(struct dune_tf));
    
    /* Create the child result, might fault.*/
    res->n_lwc = n_lwc;
    res->caller = NULL;
    res->args = NULL;

    /* Child get store the result address for now.*/
    n_lwc->tf.rax = (uint64_t) res;

    Q_INIT_ELEM(n_lwc, lk_ctx);
    Q_INIT_ELEM(n_lwc, lk_parent);
    Q_INSERT_TAIL(&(current->children), n_lwc, lk_parent);
    Q_INSERT_TAIL(contexts, n_lwc, lk_ctx);

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
    assert(tf);
    assert(lwc);
    
    /* Get the current context.*/
    lwc_struct *current = Q_GET_FRONT(contexts);
    
    /* Sanity checks.*/
    assert(current);
    assert(current->vm_mm);
    mm_struct * __current_mm = Q_GET_FRONT(mm_queue);
    assert(current->vm_mm == __current_mm);
    assert(__current_mm->pml4 == pgroot);

    Q_REMOVE(contexts, lwc, lk_ctx);
    Q_INSERT_FRONT(contexts, lwc, lk_ctx);
    
    /* Save the current point.*/
    memcpy(&(current->tf), tf, sizeof(struct dune_tf));

    /* Set the pointer to the result for when we switch back.*/
    current->tf.rax = (uint64_t) res;

    /* Do the switch*/
    *tf = lwc->tf;
    memory_switch(lwc->vm_mm);
    lwc_res_t *target_res = (lwc_res_t*) (lwc->tf.rax);
    
    if (!target_res)
        return -1;
    target_res->n_lwc = NULL;
    target_res->caller = current;
    target_res->args = args;

    /* Set return value.*/
    lwc->tf.rax = 0;
    tf->rax = 0;
    
    return 0;
}

int lwc_free(lwc_struct *lwc)
{
    assert(lwc);
    if (lwc->parent) {
        lwc_struct *parent = lwc->parent;
        Q_REMOVE(&(parent->children), lwc, lk_parent);
    }
    Q_REMOVE(contexts, lwc, lk_ctx);
    mm_free(lwc->vm_mm);
    free(lwc);
    return 0;
}