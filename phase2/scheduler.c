#include "headers/scheduler.h"
#include "headers/initial.h"
#include <uriscv/liburiscv.h>

void scheduler()
{
	if (process_count == 1 &&
	    headProcQ(&ready_queue) ==
		    ssi_pcb) { // Change when make the fake address
		HALT();
	}

	if (list_empty(&ready_queue)) {
		if (softblock_count > 0) {
			setMIE(MIE_ALL ^ MIE_MTIE_MASK);
			setSTATUS(MSTATUS_MIE_BIT);
			WAIT();
		} else { // softblock_count <= 0
			PANIC(); // DEADLOCK
		}
	}

	current_process = removeProcQ(&ready_queue);
	setTIMER(TIMESLICE);
	LDST(&current_process->p_s);
}
