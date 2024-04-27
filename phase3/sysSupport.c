#include "headers/sysSupport.h"
#include <uriscv/types.h>

static void syscall_handler(state_t *s);
static void trap_handler(state_t *s);

void general_exception_handler()
{
	// update_cpu_time();

	// get the old state from the support level struct
	state_t *s = &current_process->p_supportStruct
			      ->sup_exceptState[GENERALEXCEPT];
	unsigned int excCode = s->cause;

	if ((excCode <= 7) || (excCode > 11 && excCode < 24)) {
		trap_handler(s);
	} else if (excCode == 8) {
		syscall_handler(s);
	} else {
		// If we could not handle the exception, we panic
		PANIC();
	}
}

static void trap_handler(state_t *s)
{
	// Still TODO
}

static void syscall_handler(state_t *s)
{
	switch (s->reg_a0) {
	case SENDMESSAGE: {
		pcb_t *dest = (pcb_t *)s->reg_a1;
		unsigned int payload = s->reg_a2;

		// CHECK IF I HAVE TO SEND MSG TO MY SST_PARENT
		// (credo) SST should be in kernel-mode coz he has to 'talk'
		// with SSI
		/*if (dest == MY_SST_ADDRESS())
			dest = sst_address;
    */

		unsigned int return_val = DEST_NOT_EXIST;

		if (isPcbValid(dest)) {
			// return_val = send_message(dest, payload,
			// current_process);
		}
		// Set return value
		s->reg_a0 = return_val;

		// Increment PC to avoid sys loop
		s->pc_epc += 4;

		scheduler();
		break;
	}
	case RECEIVEMESSAGE: {
		pcb_t *sender = (pcb_t *)s->reg_a1;

		msg_t *msg = popMessage(&current_process->msg_inbox, sender);

		if (msg) { // Found message from sender
			if (current_process->do_io) {
				current_process->do_io = 0;
			}
			// deliver_message(s, msg);

			scheduler();
		} else {
			// Block the process and call the scheduler
			// blockSys();
		}
		break;
	}
	default:
		PANIC();
		break;
	}
}

/*
 * This function blocks the current process, update it's exection time
 * and calls the scheduler to select the next process to dispatch
 */
static void blockSys()
{
	current_process->p_s = *((state_t *)BIOSDATAPAGE);
	if (current_process->do_io)
		softblock_count++;
	current_process = NULL;
	scheduler();
}
