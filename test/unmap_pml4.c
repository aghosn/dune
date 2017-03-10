#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "libdune/dune.h"
#include "libdune/mm/memory.h"
#include "libdune/mm/vm_tools.h"


unsigned int counting = 0;

static int __do_nothing(ptent_t* pte, void *va, cb_info *info)
{
	//do nothing
	counting++;

	return 0;
}

int main(int argc, char* argv[])
{
	int ret = 0;

	if ((ret = dune_init(false)))
		return ret;

	if ((ret = dune_enter()))
		return ret;

	clock_t start_t, end_t;


	mm_struct* mm = memory_get_mm();
	vm_area_struct *current = NULL;
	assert(mm);

	start_t = clock();

	TAILQ_FOREACH(current, &(mm->mmap), q_areas) {
		vm_pgrot_walk(mm->pml4, (void*) current->vm_start,
			(void*) current->vm_end, &__do_nothing, NULL, NULL);
	}

	end_t = clock();

	double diff_t = (double) (end_t - start_t) / CLOCKS_PER_SEC;
	printf("Total time is %f, for %d pages.\n", diff_t, counting);
	
	return 0;
}