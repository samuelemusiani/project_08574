#include "headers/vmSupport.h"
#include "headers/utils3.h"

swap_t *swap_pool;

void initSwapStructs()
{
	// Init swap pool table
	swap_pool = (swap_t *)FLASHPOOLSTART; // TODO: Find more precise value

	for (int i = 0; i < POOLSIZE; i++) {
		swap_pool[i].sw_asid = -1;
	}
}

// Processo mutex
void mutex_proc()
{
	// TODO: Mutex process
	p_term(SELF);
}

void tlb_handler()
{
}
