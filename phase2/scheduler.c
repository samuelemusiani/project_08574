#include "headers/scheduler.h"
#include "headers/initial.h"
#include <uriscv/liburiscv.h>

static int deadlock_detector();

void scheduler()
{
	if (process_count <= 0) {
		PANIC();
	} else if (process_count == 1 &&
		   headProcQ(&ready_queue) ==
			   ssi_pcb) { // Change when make the fake address
		HALT();
	} else if (process_count > 0) {
		if (softblock_count > 0) {
			setMIE(MIE_ALL ^ MIE_MTIE_MASK);
			setSTATUS(MSTATUS_MIE_BIT);
			WAIT();
		} else if (deadlock_detector()) {
			PANIC(); // DEADLOCK
		}
	}

	current_process = removeProcQ(&ready_queue);
	setTIMER(TIMESLICE);
	LDST(&current_process->p_s);
}

static int deadlock_detector()
{
	// Deadlock for μPandOS is defined as when the Process Count > 0
	// and the Soft-block Count is 0. Take an appropriate deadlock detected
	// action; invoke the PANIC BIOS service/instruction [Section 7.3.6-pops].
	//
	// We can't panic, the first state of the kernel match deadlock.
	return 0;
}
