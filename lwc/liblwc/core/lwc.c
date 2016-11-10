#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <pthread.h>

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

static mm_struct* lwc_apply_mm(lwc_rsrc_spec *mod, mm_struct *o)
{
    assert(mod && o);
    mm_struct *copy = mm_copy(o);
    if (!copy)
        goto err;

    //TODO: apply the mods.
    return copy;

err:
    return NULL;
}


lwc_struct* lwc_create(lwc_rsrc_spec *mod)
{
    mm_struct *copy = NULL;
    lwc_struct *n_lwc = NULL, *current = NULL;

    /* The current context.*/
    current = Q_GET_FRONT(contexts);
    if (!current)
        goto err;

    /* Quick copy*/
    if (!mod || mod->ranges->head == NULL) {
        copy = current->vm_mm;
        current->vm_mm->ref++;
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
    //TODO: init the dune_tf.
    Q_INIT_ELEM(n_lwc, lk_ctx);
    Q_INIT_ELEM(n_lwc, lk_parent);
    Q_INSERT_TAIL(&(current->children), n_lwc, lk_parent);
    Q_INSERT_TAIL(contexts, n_lwc, lk_ctx);
    return n_lwc;
err:
    if (n_lwc)
        lwc_free(n_lwc);
    return NULL;
}

int lwc_free(lwc_struct *lwc)
{
    //TODO: implement
    return 0;
}


