#include "headers/vmSupport.h"

void initSwapStructs()
{
	// Init swap pool table
	swap_pool = (swap_t *)FLASHPOOLSTART; // TODO: Find more precise value

	for (int i = 0; i < POOLSIZE; i++) {
		swap_pool[i].sw_asid = -1;
	}
}

void tlb_handler()
{
}
