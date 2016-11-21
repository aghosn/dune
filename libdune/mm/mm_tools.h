#ifndef __LIBDUNE_MM_MM_TOOLS_H__
#define __LIBDUNE_MM_MM_TOOLS_H__

#include "mm_types.h"

int mm_cow(	mm_struct *original,
			mm_struct *copy,
			vm_addrptr start,
			vm_addrptr end,
			bool apply);

int mm_shared(	mm_struct *original,
				mm_struct *copy,
				vm_addrptr start,
				vm_addrptr end,
				bool apply);


//TODO: remove, for debugging.
int mm_into_root(mm_struct *mm);
int mm_count_entries(mm_struct *mm);
int mm_check_regions(mm_struct *mm);
#endif /*__LIBDUNE_MM_MM_TOOLS_H__*/