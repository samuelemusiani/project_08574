#include "headers/scheduler.h"
#include "headers/initial.h"
#include "headers/exceptions.h"
#include <uriscv/liburiscv.h>

/*
 * The scheduler function is responsible for selecting the next process to
 * dispatch by checking the state of the ready queue.
 */
void scheduler()
{
	/*
	 * If there is only one process in the process_count and the ssi is
	 * blocked on a receive syscall (so it's not in the ready queue), the
	 * scheduler halts the system.
	 */
	if (process_count == 1 && !searchPcb(&ready_queue,
					     ssi_pcb)) {
		HALT();
	}

	if (list_empty(&ready_queue)) {
		/*
		 * If there are soft blocked processes, the scheduler enable
		 * interrupts and disable the PLT timer, then enter a wait
		 * state.
		 */
		if (softblock_count > 0) {
			setMIE(MIE_ALL ^ MIE_MTIE_MASK);
			setSTATUS(1 << MSTATUS_MIE_BIT);
			WAIT();
		} else {
			// If there are no soft blocked processes, the function
			// panics and indicates a deadlock.
			PANIC(); // DEADLOCK
		}
	}

	current_process = removeProcQ(&ready_queue);
	setTIMER(TIMESLICE * (*((cpu_t *)TIMESCALEADDR)));
	// Get current TOD timer value and store in the global variable
	// tod_timer
	STCK(tod_timer);
	// Perform a Load Processor State on the p_s stored in PCB of the
	// current_process.
	LDST(&current_process->p_s);
}
