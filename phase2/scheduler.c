#include "headers/scheduler.h"
#include "headers/initial.h"
#include "headers/exceptions.h"
#include <uriscv/liburiscv.h>

static void load_state(state_t *s);

/*
 * The scheduler is responsible for selecting the next process to dispatch. If
 * the current process is not NULL, the scheduler dispatches the current
 * process; otherwise it will check the state of the ready queue in order to
 * select the next one.
 */
void scheduler()
{
	if (current_process != NULL)
		load_state((state_t *)BIOSDATAPAGE);

	/*
	 * If there is only one process in the process_count and
	 * the ssi is blocked on a receive syscall (so it's not
	 * in the ready queue), the scheduler halts the system.
	 */
	if (process_count == 1 && !searchPcb(&ready_queue, ssi_pcb)) {
		HALT();
	}

	if (list_empty(&ready_queue)) {
		/*
		 * If there are soft blocked processes, the scheduler enables
		 * interrupts and disables the PLT timer, then enter a wait
		 * state.
		 */
		if (softblock_count > 0) {
			setMIE(MIE_ALL ^ MIE_MTIE_MASK);
			setSTATUS(1 << MSTATUS_MIE_BIT);
			WAIT();
		} else {
			// If there are no soft-blocked processes, the function
			// panics and indicates a deadlock.
			PANIC();
		}
	}

	current_process = removeProcQ(&ready_queue);
	load_state(&current_process->p_s);
}

/*
 * The scheduler only selects the process to dispatch, this function does the
 * actual load *
 */
static void load_state(state_t *s)
{
	setTIMER(TIMESLICE * (*((cpu_t *)TIMESCALEADDR)));
	// Get the current TOD timer value and store in the global variable
	// tod_timer
	STCK(tod_timer);
	LDST(s);
}
