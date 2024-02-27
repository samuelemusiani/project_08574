#include "headers/interrupts.h"
#include "headers/initial.h"
#include "headers/scheduler.h"
#include "headers/exceptions.h"
#include "headers/ssi.h"
#include <uriscv/arch.h>

/* QUESTIONS:
 * 1. What if the PCB waiting for the device's interrupt was terminated?
 * 2. How do I handle multiple interrupt lines?
 *    Do I have to recall the interrupt_handler myself?
*/

static void plt_interrupt_handler();
static void it_interrupt_handler();
static void device_interrupt_handler(unsigned int iln);

void interrupt_handler()
{
	/*
	 * We can only handle one interrupt at a time, the one with the highest priority;
	 * the interrupt with the highest priority is the lowest device number
	 * with the lowest interrupt line number.
	*/
	unsigned int mip = getMIP();
	if (mip & 1 << IL_TIMER)
		it_interrupt_handler();
	else if (mip & 1 << IL_CPUTIMER)
		plt_interrupt_handler();
	else {
		int iln = DEV_IL_START;
		int max_il = DEV_IL_START + N_EXT_IL;
		while (iln < max_il && !(mip & 1 << iln)) {
			iln++;
		}

		if (iln == max_il)
			PANIC();
		device_interrupt_handler(iln);
	}
}

static void device_interrupt_handler(unsigned int iln)
{
	/*
	 * CDEV_BITMAP_ADDR(IntlineNo) is the address of the interrupting devices bitmap.
	 * 8 bit, each bit represents a device.
	 * We need the device with the lower number that has 1 on the bit corresponding to it.
	*/
	int bitmap = *(char *)CDEV_BITMAP_ADDR(iln);
	int dev_n = 0;
	int tmp1 = bitmap & (1 << dev_n);
	int cond = !tmp1;
	while (dev_n < N_DEV_PER_IL && cond) {
		dev_n++;
	}
	if (dev_n == N_DEV_PER_IL)
		PANIC();

	devreg_t *devAddrBase = (devreg_t *)DEV_REG_ADDR(iln, dev_n);
	char statusCode;
	if (iln == IL_TERMINAL) {
		/*
     * For the duration of the operation the sub-device’s status is “Device Busy.”
     * Upon completion of the operation an interrupt is raised and an appropriate
     * status code is set in TRANSM_STATUS or RECV_STATUS respectively;
     * “Character Transmitted/Received” for successful completion (code 5) or
     * one of the error codes (codes 0, 2, 4).
     *
     * ??? Is "device busy" an error code ???
    */
		char transm_status = devAddrBase->term.transm_status;
		if (transm_status != 1 && transm_status != 3) {
			statusCode = transm_status;
			devAddrBase->term.transm_command = ACK;
		} else {
			statusCode = devAddrBase->term.recv_status;
			devAddrBase->term.recv_command = ACK;
		}
	} else {
		statusCode = devAddrBase->dtp.status;
		devAddrBase->dtp.command = ACK;
	}
	/*
	 * Send a message to the ssi with the status code; it will be its job
	 * to unblock the process, set the status code in the process' a0 register
	 * and insert it in the ready queue.
	*/
	interrupt_handler_io_msg_t msg = { .fields.service = 0,
					   .fields.device_type =
						   EXT_IL_INDEX(iln),
					   .fields.device_number = dev_n,
					   .fields.status = statusCode };
	send_message_to_ssi(msg.payload);
	LDST((state_t *)BIOSDATAPAGE);
}

static void plt_interrupt_handler()
{
	current_process->p_s = *(state_t *)BIOSDATAPAGE;
	insertProcQ(&ready_queue, current_process);
	current_process = NULL;
	scheduler();
}

static void it_interrupt_handler()
{
	LDIT(PSECOND);
	interrupt_handler_io_msg_t msg = { .fields.service = 1 };
	send_message_to_ssi(msg.payload);
	LDST((state_t *)BIOSDATAPAGE);
}
