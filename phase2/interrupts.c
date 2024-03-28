#include "headers/interrupts.h"
#include "headers/initial.h"
#include "headers/scheduler.h"
#include "headers/exceptions.h"
#include "headers/ssi.h"
#include <uriscv/arch.h>

static void plt_interrupt_handler();
static void it_interrupt_handler();
static void device_interrupt_handler(unsigned int iln);

/*
 * Handles interrupts by checking the interrupt priority and
 * calling the corresponding interrupt handler functions.
 * We can only handle one interrupt at a time, the one with the highest
 * priority.
 */
void interrupt_handler()
{
	// MIP register contains the pending interrupts
	// Three types of interrupts: timer, cpu timer, and devices
	unsigned int mip = getMIP();

	// interr has 0 in all bits except in the 3rd, 7th, 17-21th bits
	// 5 types of device from 17 to 21
	unsigned int interr = 1 << IL_TIMER | 1 << IL_CPUTIMER |
			      31 << DEV_IL_START;

	while (mip & interr) {
		if (mip & 1 << IL_TIMER) {
			mip &= ~(1 << IL_TIMER); // clear the interrupt bit
			it_interrupt_handler();
		} else if (mip & 1 << IL_CPUTIMER) {
			mip &= ~(1 << IL_CPUTIMER);
			plt_interrupt_handler();
		} else {
			/*
			 * For devices, the interrupt with the highest priority
			 * is the lowest device number with the lowest interrupt
			 * line number.
			 */
			int iln = DEV_IL_START;
			int max_il = DEV_IL_START + N_EXT_IL;
			while (iln < max_il && !(mip & 1 << iln)) {
				iln++;
			}

			if (iln == max_il)
				PANIC();
			mip &= ~(1 << iln);
			device_interrupt_handler(iln);
		}
	}
	scheduler();
}
/*
 * This function handles device interrupts for a specific interrupt line.
 */
static void device_interrupt_handler(unsigned int iln)
{
	/*
	 * CDEV_BITMAP_ADDR(IntlineNo) is the address of the interrupting
	 * devices bitmap. 8 bit, each bit represents a device. We need the
	 * device with the lower number that has 1 on the bit corresponding to
	 * it.
	 */
	int bitmap = *(char *)CDEV_BITMAP_ADDR(iln);
	int dev_n = 0;
	// The folling code is needed as the current version of uriscv have a
	// bug on the instruction !(bitmap & (1 << dev_n))
	int tmp1 = bitmap & (1 << dev_n);
	int cond = !tmp1;
	while (dev_n < N_DEV_PER_IL && cond) {
		dev_n++;
		tmp1 = bitmap & (1 << dev_n);
		cond = !tmp1;
	}
	if (dev_n == N_DEV_PER_IL)
		PANIC();

	devreg_t *devAddrBase = (devreg_t *)DEV_REG_ADDR(iln, dev_n);
	char statusCode;

	int device_is_terminal; // The interrupt is from a terminal
	int terminal_transm;	// The interrupt is from a transm sub-terminal
	if (IL_TERMINAL <= iln && iln < IL_TERMINAL + DEVPERINT) {
		device_is_terminal = 1;
		/*
		 * For the duration of the operation, the sub-device’s status is
		 * “Device Busy.” Upon completion of the operation, an interrupt
		 * is raised and an appropriate status code is set in
		 * TRANSM_STATUS or RECV_STATUS respectively; “Character
		 * Transmitted/Received” for successful completion (code 5) or
		 * one of the error codes (codes 0, 2, 4).
		 */
		char transm_status = devAddrBase->term.transm_status;
		if (transm_status != 1 && transm_status != 3) {
			statusCode = transm_status;
			devAddrBase->term.transm_command = ACK;
			terminal_transm = 1;
		} else {
			statusCode = devAddrBase->term.recv_status;
			devAddrBase->term.recv_command = ACK;
			terminal_transm = 0;
		}
	} else {
		device_is_terminal = 0;
		statusCode = devAddrBase->dtp.status;
		devAddrBase->dtp.command = ACK;
	}

	interrupt_handler_io_msg_t msg = {
		.fields.service = 0,			 // doio
		.fields.device_type = EXT_IL_INDEX(iln), // type of device
		.fields.device_number = dev_n,		 // number of device
		.fields.status = statusCode		 // device's status
	};

	if (device_is_terminal) {
		// The following is a bug. I've reported the bug but for now
		// it's not fixed.
		// msg.fields.device_number |=
		// 	terminal_transm ? SUBTERMINAL_TRANSM : SUBTERMINAL_RECV;
		if (terminal_transm)
			msg.fields.device_number |= SUBTERMINAL_TRANSM;
		else
			msg.fields.device_number |= SUBTERMINAL_RECV;
	}

	/*
	 * Send a message to the ssi with the status code; it will be its job
	 * to unblock the process
	 *
	 * Avoiding syscall because it would generate an exception and
	 * ovveride the Nucleous stack memory.
	 * The payload is not a pointer, but an unsigned int that stores all the
	 * informations that the SSI will need. A pointer is avoided because
	 * it would point to an address in the interrupt handler function
	 * that could be overwritten by the next interrupt.
	 */
	send_message_to_ssi(msg.payload);
}

/*
 * Every 5ms the PLT raises an interrupt
 * The function save the state of the current process
 * and put it in the ready queue
 */
static void plt_interrupt_handler()
{
	current_process->p_s = *(state_t *)BIOSDATAPAGE;
	insertProcQ(&ready_queue, current_process);
	current_process = NULL;
}

/*
 * Load the interval timer with 100ms
 * Send a message to the ssi with the service field set to 1
 * that will unblock all PCBs waiting a PseduoClock Tick
 */
static void it_interrupt_handler()
{
	LDIT(PSECOND);
	interrupt_handler_io_msg_t msg = { .fields.service = 1 };
	send_message_to_ssi(msg.payload);
}
