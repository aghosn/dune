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

    lwc_root->children = malloc(sizeof(l_lwc));
    if (!lwc_root->children) {
        ret = -ENOMEM;
        goto err;
    }

    Q_INIT_ELEM(lwc_root, lk_ctx);
    Q_INIT_ELEM(lwc_root, lk_parent);
    Q_INIT_HEAD(lwc_root->children);
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
    if (lwc_root && lwc_root->children)
        free(lwc_root->children);
    if (lwc_root)
        free(lwc_root);
    if (contexts)
        free(contexts);
    return ret;
}



