#ifndef __LWC_H__
#define __LWC_H__

#include <sys/types.h>
#include <dune.h>


#define NB_ENTRIES_PTE 512


/*Helper functions*/
ptent_t* deep_copy_pgroot(ptent_t *pgroot, ptent_t *cppgroot);


/*lwC types*/
typedef uint64_t* lw_context;

typedef struct lwC_desc {
	lw_context *n;
	uint64_t caller;
	void	*args;
} lwC_desc_t;

typedef struct lwC_switch {
	uint64_t caller;
	void *args;
} lwC_switch_t;

/*lwC's API*/

//Creation and switching
lwC_desc_t lwCreate(void);
lwC_switch_t lwSwitch(void);

//Modifying rights
void lw_restrict(void);
void lw_share_memory(void);
void lw_syscall(void); //TODO not sure about this.

#endif