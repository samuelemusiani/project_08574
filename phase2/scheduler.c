#include "headers/scheduler.h"
#include "headers/initial.h"
#include <uriscv/liburiscv.h>

void scheduler()
{
	if (list_empty(&ready_queue)) {
		if (process_count <= 0) {
			PANIC();
		} else if (process_count == 1 &&
			   headProcQ(&ready_queue) ==
				   ssi_pcb) { // Change when make the fake address
			HALT();
		} else {
			if (softblock_count > 0) {
				setMIE(MIE_ALL ^ MIE_MTIE_MASK);
				setSTATUS(MSTATUS_MIE_BIT);
				WAIT();
			} else { // process_count > 0 && softblock_count <= 0
				PANIC(); // DEADLOCK
			}
		}
	} else {
		current_process = removeProcQ(&ready_queue);
		setTIMER(TIMESLICE);
		LDST(&current_process->p_s);
	}
}
