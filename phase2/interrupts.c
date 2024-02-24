#include "headers/interrupts.h"
#include "headers/initial.h"
#include "headers/scheduler.h"
#include "headers/exceptions.h"
#include "headers/ssi.h"

void pltInterrupt_handler();
void intervalTimerInterrupt_handler();
void nonTimerInterrupt_handler(unsigned int IntlineNo);

void interrupt_handler()
{
	// we can only handle one interrupt at a time, the one with the highest priority;
	// the interrupt with the highest priority is
	// the lowest device number with the lowest interrupt line number
	unsigned int mip = getMIP();
	if (mip & 1 << IL_TIMER) // interval timer
		intervalTimerInterrupt_handler();
	else if (mip & 1 << IL_CPUTIMER) // plt
		pltInterrupt_handler();
	else if (mip << 11) // non timer interrupt
	{
		char IntlineNo = DEV_IL_START;
		char found = 0;
		while (IntlineNo < DEV_IL_START + N_EXT_IL && !found) {
			if (mip & 1 << IntlineNo)
				found = 1;
			else
				IntlineNo++;
		}
		if (!found)
			PANIC();
		nonTimerInterrupt_handler(IntlineNo);
	} else
		PANIC();
}

void nonTimerInterrupt_handler(unsigned int IntlineNo)
{
	// CDEV_BITMAP_ADDR(IntlineNo) is the address of the interrupting devices bitmap
	// 8 bit, each bit represents a device. We need the device with the lower number
	// that has 1 on the bit corresponding to it
	char *bitmap = (char *)CDEV_BITMAP_ADDR(IntlineNo);
	char found = 0;
	unsigned int device = 0, i = 0;
	while (!found && i < N_DEV_PER_IL) {
		if (*bitmap & 1 << i)
			found = 1;
		else
			i++;
	}
	if (!found)
		PANIC();

	unsigned int devList[N_DEV_PER_IL] = { DEV0ON, DEV1ON, DEV2ON, DEV3ON,
					       DEV4ON, DEV5ON, DEV6ON, DEV7ON };
	devreg_t *devAddrBase =
		(devreg_t *)DEV_REG_ADDR(IntlineNo, devList[device]);
	char statusCode =
		devAddrBase->dtp.status; // is this the right register?
	devAddrBase->dtp.status = ACK;

	// send a message to the ssi with the status code; it will be its job
	// to unblock the process, set the status code in the process' a0 register
	// and insert it in the ready queue.
	interrupt_handler_io_msg_t msg = { .fields.service = 0,
					   .fields.device_type =
						   EXT_IL_INDEX(IntlineNo),
					   .fields.il = device,
					   .fields.status = statusCode };
	send_message_to_ssi(msg.payload);
	LDST((state_t *)BIOSDATAPAGE);
}

void pltInterrupt_handler()
{
	current_process->p_s = *(state_t *)BIOSDATAPAGE;
	insertProcQ(&ready_queue, current_process);
	current_process = NULL;
	scheduler();
}

void intervalTimerInterrupt_handler()
{
	LDIT(PSECOND);
	interrupt_handler_io_msg_t msg = { .fields.service = 1,
					   .fields.device_type = 0,
					   .fields.il = 0,
					   .fields.status = 0 };
	send_message_to_ssi(msg.payload);
	LDST((state_t *)BIOSDATAPAGE);
}
