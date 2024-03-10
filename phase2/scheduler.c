#include "headers/scheduler.h"
#include "headers/initial.h"
#include "headers/exceptions.h"
#include <uriscv/liburiscv.h>

/**
 * The scheduler function is responsible for selecting the next process to dispatch by checking the state of the ready queue
 * 
 * If there is only one process in the process_count and the ssi is blocked 
 * on a receive syscall (so it's not in the ready queue), the scheduler 
 * halts the system.
 * If the ready queue is empty, the function checks if there are any soft 
 * blocked processes.
 * If there are soft blocked processes, the scheduler enable interrupts
 * and disable the PLT timer, then enter a wait state.
 * If there are no soft blocked processes, the function panics and 
 * indicates a deadlock.
 * 
 * Remove the PCB from the ready queue and store the pointer in the current_process. 
 * Load 5 milliseconds into the PLT 
 * Get current TOD timer value and store in the global variable tod_timer
 * Perform a Load Processor State on the p_s stored in PCB of the current_process.
 * 
 */
void scheduler()
{
	if (process_count == 1 && !searchPcb(&ready_queue, ssi_pcb_real)) {
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
