#include "headers/scheduler.h"
#include "headers/initial.h"
#include "headers/exceptions.h"
#include <uriscv/liburiscv.h>

void scheduler()
{
	// Change when make the fake address
	if (process_count == 1 && !searchPcb(&ready_queue, ssi_pcb)) {
		HALT();
	}

	if (list_empty(&ready_queue)) {
		if (softblock_count > 0) {
			setMIE(MIE_ALL ^ MIE_MTIE_MASK);
			setSTATUS(1 << MSTATUS_MIE_BIT);
			WAIT();
		} else {	 // softblock_count <= 0
			PANIC(); // DEADLOCK
		}
	}

	current_process = removeProcQ(&ready_queue);
	setTIMER(TIMESLICE * (*((cpu_t *)TIMESCALEADDR)));
	STCK(tod_timer);
	LDST(&current_process->p_s);
}
