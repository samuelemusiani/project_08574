#include "headers/interrupts.h"
#include "headers/initial.h"
#include "headers/scheduler.h"
#include "headers/exceptions.h"
#include "headers/ssi.h"
#include <uriscv/arch.h>

static void plt_interrupt_handler();
static void it_interrupt_handler();
static void device_interrupt_handler(unsigned int iln);

void interrupt_handler()
{
	/*
	 * We can only handle one interrupt at a time, the one with the highest
	 * priority; the interrupt with the highest priority is the lowest
	 * device number with the lowest interrupt line number.
	 */
	unsigned int mip = getMIP();
	// if interr > 0, there is still at least one interrupt to handle
	unsigned int interr = 1 << IL_TIMER | 1 << IL_CPUTIMER |
			      31 << DEV_IL_START;
	while (mip & interr) {
		if (mip & 1 << IL_TIMER) {
			mip &= ~(1 << IL_TIMER);
			it_interrupt_handler();
		} else if (mip & 1 << IL_CPUTIMER) {
			mip &= ~(1 << IL_CPUTIMER);
			plt_interrupt_handler();
		} else {
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
	if (current_process == NULL)
		scheduler();
	else
		LDST((state_t *)BIOSDATAPAGE);
}

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
	if (iln == IL_TERMINAL) {
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

	// Send a message to the ssi with the status code; it will be its job
	// to unblock the process
	interrupt_handler_io_msg_t msg = { .fields.service = 0,
					   .fields.device_type =
						   EXT_IL_INDEX(iln),
					   .fields.device_number = dev_n,
					   .fields.status = statusCode };

	if (device_is_terminal) {
		msg.fields.device_number |=
			terminal_transm ? SUBTERMINAL_TRANSM : SUBTERMINAL_RECV;
	}
	send_message_to_ssi(msg.payload);
}

static void plt_interrupt_handler()
{
	current_process->p_s = *(state_t *)BIOSDATAPAGE;
	insertProcQ(&ready_queue, current_process);
	current_process = NULL;
}

static void it_interrupt_handler()
{
	LDIT(PSECOND);
	interrupt_handler_io_msg_t msg = { .fields.service = 1 };
	send_message_to_ssi(msg.payload);
}
