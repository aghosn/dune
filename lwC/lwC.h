#ifndef __LWC_H__
#define __LWC_H__

#include <sys/types.h>
#include <dune.h>


#define NB_ENTRIES_PTE 512


/*Helper functions*/
ptent_t* deep_copy_pgroot(ptent_t *pgroot, ptent_t *cppgroot);

/*lwC Sharing options*/
#define LWC_COW 	0
#define LWC_UNMAP	1
#define LWC_SHARED 	2

/*lwC types*/

/*TODO not sure what they are supposed to be*/
//See section 3.8 in the paper.
//Non attributable is delivered upon next switch.
#define LWC_SHARESIGNALS -1

//in resource_spec to allow sharing parent's capabilities.
#define LWC_MAY_ACCESS -3

//Redirect unallowed child's syscalls to parent.
//Specified during the lwC's creation.
//Parent gets system call number and args.
#define LWS_SYSTRAP -2

//Supposed to be an fd.
//Associated with capabilities.
typedef uint64_t* lwc_context_t;

//TODO array of C unions where element is either range of fd
//virtual addresses or credentials/ syscall numbers.
//for each range, can specify one of these options:
//LWC_COW -> logical copy of range of ressources.
//LWC_SHARED -> shared between child and parent.
//LWC_UNMAP -> not mapped inside children.
typedef uint64_t lwc_resource_spec;

typedef struct __lwc_desc {
	lwc_context_t *newC;
	uint64_t caller;
	void	*args;
} lwc_desc_t;


typedef struct __lwc_switch {
	uint64_t caller;
	void *args;
} lwc_switch_t;

/*lwC's API*/

//Creation and switching

/**
 * @brief      	Similar to posix fork, creates a copy of the VA etc.
 * except descriptors. 
 * 				
 * @param[in]  	specs
 *
 * @return     	{fd_new, -1, args} at first call.
 * 				{parent, caller, args} when first switch.
 * 				
 * @TODO 	Needs FLAGS as argument
 * 			Needs per thread information cloned too.
 * 			Mappings must be copy-on-write.
 * 			Need to pass on the stack, code, synch variables and other deps.
 */
lwc_desc_t lwc_create(lwc_resource_spec specs); // -> equivalent to fork.

/**
 * @brief      
 *
 * @return     { description_of_the_return_value }
 * 
 * @TODO		Need (targert, args) as arguments.
 * 				Compared to process switch, kernel thread switch
 * 				POSIX user thread switch (getContext setContext).
 */
lwc_switch_t lwc_switch(void); // -> equivalent to yield.

/**
 * @brief      { function_description }
 *
 * @param[in]  	l 
 * 
 * @param[in]  	specs 
 * Restricts the set of resources that may be overlayed or accessed 
 * by any context that holds a descriptor to lwC l.
 * Can be virtual memory, syscall numbers, or file descriptors.
 * 
 * @TODO		Should change the name.
 * 				TODO understand if it is black list or white list.
 * 				The paper is not clear about that.
 */
void lwc_restrict(lwc_context_t l, lwc_resource_spec specs); // probably memprotect

/**
 * @brief      		  Map memory from foreign lwC to current lwC.
 * Dynamically maps ("overlays" in paper) resources from another lwC 
 * into the current address space.
 * 
 * @param[in]  l      { parameter_description }
 * 
 * @param[in]  specs  Specifies which resource type (memory or file descriptor) 
 * are to be shared, and if it should be copied or shared.
 * 
 * @TODO			  Called lwc_overlay in paper.
 * 					  Succeeds only if caller has access capabilities.
 * 					  A successful call unmaps existing ressources at
 * 					  specified addresses in the caller's address space.
 */
void lwc_share_memory(lwc_context_t l, lwc_resource_spec specs);

/**
 * @brief      Enables an lwC to execute syscalls on behalf of another (target).
 * The call succeeds if the caller has the capabilities.
 * As if the target had executed the syscall, except that it returns to the
 * calling context.
 * 
 * @param[in]  tgt      The target context of the system call.
 * 
 * @param[in]  mask		Enables the caller to specify aspects of its own context
 * that are to be placed during the duration of the call (e.g., file table,
 * memory space, credentials, or combination of them).
 * 
 * @param[in]  syscall  Syscall id TODO 
 * @param      args     Arguments for the syscall.
 * 
 * @TODO		Need to find the proper types for this.
 * 				What does it look like in linux?
 * @Example		Enable lwC to append bytes or read a file for which it does not
 * 				have any access.
 */
void lwc_syscall(lwc_context_t tgt, uint64_t mask, uint64_t syscall, void* args); //TODO not sure about this.

#endif