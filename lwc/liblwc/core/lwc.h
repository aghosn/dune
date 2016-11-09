#ifndef __LWC_H__
#define __LWC_H__

#include <sys/types.h>
#include <dune.h>

#include "../tools/vq.h"
#include <mm/mm_types.h>

/*lwC create options.*/

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

/*lwC types*/
//TODO define
typedef uint64_t lwc_syscall_t;
//TODO define
typedef uint64_t* lwc_file_descriptor_t;
//TODO define
typedef uint64_t lwc_credentials_t;

/*******************************************************************************
 *						Definition of a context.
 ******************************************************************************/

//Supposed to be an fd.
//Associated with capabilities.
typedef struct lwc_context_t {
	mm_struct *vm_mm;
	struct dune_tf tf;
	Q_NEW_LINK(lwc_context_t) link_ctx;
} lwc_context_t;

//Root context.
extern lwc_context_t __lwc_root;

/*Descriptive types in the resource specs.*/
typedef enum lwc_type_tag_t {
	VIRTUAL_ADDRESS = 1,
	FILE_DESCRIPTOR = 2,
	CREDENTIAL = 3,
	SYSCALL = 4
} lwc_type_tag_t;

/*lwC Sharing options*/
typedef enum lwc_range_option_t {
	LWC_COW = 1,
	LWC_SHARED = 2,
	LWC_UNMAP = 3
} lwc_range_option_t;

/*Actual types inside the resource specs.*/
typedef union lwc_resource_type_t {
	void* address;
	lwc_file_descriptor_t fd; 		
	lwc_syscall_t syscall; 	//Not sure about this one.
	lwc_credentials_t credentials;//Not sure either.
} lwc_resource_type_t;

/*Defines a range as an entry in resource specs.*/
typedef struct lwc_range_t {
	lwc_resource_type_t start;
	lwc_resource_type_t end;
	
	lwc_type_tag_t type;
	lwc_range_option_t options;
} lwc_range_t;

//Description in paper: array of C unions where element is either range of fd
//virtual addresses or credentials/ syscall numbers.
//for each range, can specify one of these options:
//LWC_COW -> logical copy of range of ressources.
//LWC_SHARED -> shared between child and parent.
//LWC_UNMAP -> not mapped inside children.
typedef struct lwc_resource_spec_t {
	size_t length;
	lwc_range_t *entries;
} lwc_resource_spec_t;


/*Return type for lwc_create and lwc_switch*/
typedef struct lwc_result_t {
	lwc_context_t *newC;
	uint64_t caller;
	void	*args;
} lwc_result_t;

/*lwC's API*/

/**
 * @brief      Initializes the lwC.
 * 
 * @TODO		Create lwC root.
 * 				Register new dune pgfault handler.
 */
void lwc_init(int argc, char *argv[]);

//Creation and switching
/**
 * @brief      	Similar to posix fork, creates a copy of the VA etc.
 * except descriptors. 
 * 				
 * @param[in]  	specs
 * @param[in]  	options
 *
 * @return     	{fd_new, -1, args} at first call.
 * 				{parent, caller, args} when first switch.
 * 				
 * @TODO 	Needs FLAGS as argument
 * 			Needs per thread information cloned too.
 * 			Mappings must be copy-on-write.
 * 			Need to pass on the stack, code, synch variables and other deps.
 *
 * @Implementation
 * Take current lwc (how?) and copy VMAs.
 *	1. Need to ID VMA that is kernel and shared.
 *	2. Need to make COW on the user space.
 *	3. Prepare stack for the return from the child.
 *		3.1. Push all registers on the stack. 
 *		3.2. In child, push parent caller and args.
 *		3.3. In child, rsp and rip must be there.
 *		3.4. Must return from there.
 *	4. Opt:Add lwc to some parent-only accessible structure and return to parent.
 */
lwc_result_t lwc_create(lwc_resource_spec_t specs, uint64_t options); // -> equivalent to fork.

/**
 * @brief      Switch to target context l with specified arguments.
 *
 * @param[in]  l     Target context.
 * @param      args  
 *
 * @return     lwc_result_t: {NULL, caller, args}
 * caller points to the target.
 * args is a pointer to the args passed.
 * 
 * @TODO		Need (targert, args) as arguments.
 * 				Compared to process switch, kernel thread switch
 * 				POSIX user thread switch (getContext setContext).
 * 				newC is null in the result.
 */
lwc_result_t lwc_switch(lwc_context_t l, void* args); // -> equivalent to yield.

//API for memory access
/**
 * @brief      Allows fine-grained control over resource allocation in l.
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
void lwc_restrict(lwc_context_t l, lwc_resource_spec_t specs); // probably memprotect

/**
 * @brief      		  Map memory from foreign lwC to current lwC.
 * Dynamically maps ("overlays" in paper) resources from another lwC 
 * into the current address space.
 * 
 * @param[in]  l      lwC containing the memory to map.
 * 
 * @param[in]  specs  Specifies which resource type (memory or file descriptor) 
 * are to be shared, and if it should be copied or shared.
 * 
 * @TODO			  Called lwc_overlay in paper.
 * 					  Succeeds only if caller has access capabilities.
 * 					  A successful call unmaps existing ressources at
 * 					  specified addresses in the caller's address space.
 */
void lwc_share_memory(lwc_context_t l, lwc_resource_spec_t specs);

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
void lwc_syscall(lwc_context_t tgt, uint64_t mask, uint64_t syscall, void* args); //TODO not sure about this. Do not implement for the moment.

void do_syscall(struct dune_tf *tf, uint64_t sysnr);

#endif