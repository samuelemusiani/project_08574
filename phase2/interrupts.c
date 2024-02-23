#include "headers/interrupts.h"
#include "headers/initial.h"
#include "headers/scheduler.h"
#include "uriscv/arch.h"

void pltInterrupt_handler();
void intervalTimerInterrupt_handler();
void nonTimerInterrupt_handler(unsigned int IntlineNo);

void interrupt_handler()
{
	unsigned int cause = getCAUSE();
	// we can only handle one interrupt at a time, the one with the highest priority;
	// the interrupt with the highest priority is
	// the lowest device number with the lowest interrupt line number
	unsigned int found = 0;
	unsigned int line = 0;
	// !!! The cause register constant for the ethernet device is missing, maybe it's this one??
	unsigned int const ETHERNETINTERRUPT = 0x00002000;
	unsigned int causeRegConst[7] = { LOCALTIMERINT,     TIMERINTERRUPT,
					  DISKINTERRUPT,     FLASHINTERRUPT,
					  ETHERNETINTERRUPT, PRINTINTERRUPT,
					  TERMINTERRUPT };
	while (!found && line < 7) {
		if (cause &
		    causeRegConst[line -
				  1]) // Specs told me this, I don't think works :(
			found = 1;
		else
			line++;
	}
	line++; // we can ignore the first line, shift the line numbers by 1
	if (!found)
		PANIC();

	if (line == 1) // plt interrupt
		pltInterrupt_handler();
	else if (line == 2) // interval timer interrupt
		intervalTimerInterrupt_handler();
	else // device interrupt
		nonTimerInterrupt_handler(line + 17);
}

void nonTimerInterrupt_handler(unsigned int IntlineNo)
{
	// CDEV_BITMAP_ADDR(IntlineNo) is the address of the interrupting devices bitmap
	// 8 bit, each bit represents a device. We need the device with the lower number
	// that has 1 on the bit corresponding to it
	unsigned int *bitmap = (unsigned int *)CDEV_BITMAP_ADDR(IntlineNo);
	unsigned int found = 0, device = 0, i = 0;
	while (!found && i < 8) {
		if (*bitmap & 1 << i)
			found = 1;
		else
			i++;
	}
	if (!found)
		PANIC();

	unsigned int devList[8] = { DEV0ON, DEV1ON, DEV2ON, DEV3ON,
				    DEV4ON, DEV5ON, DEV6ON, DEV7ON };
	devreg_t *devAddrBase =
		(devreg_t *)DEV_REG_ADDR(IntlineNo, devList[device]);
	unsigned int statusCode = devAddrBase->dtp.status;
	if (statusCode) // compiler angry because I don't use statusCode yet :(
		;
	devAddrBase->dtp.status = ACK;
	// send a message to the ssi with the status code; it will be its job
	// to unblock the process, set the status code in the process' a0 register
	// and insert it in the ready queue.
	// DO NOT use a syscall

	// TODO

	LDST((state_t *)BIOSDATAPAGE); // return to the interrupted process
}

void pltInterrupt_handler()
{
	setTIMER(TIMESLICE); // Isn't that a scheduler's job?

	// Do I have to directly manage the process? Or do I have to call the ssi?
	/*
  currentProcess->p_s = (state_t*)BIOSDATAPAGE;
  insertProcQ(&ready_queue, currentProcess);
	outProcQ(&pcb_blocked_on_clock, currentProcess);
	softblock_count--;
  */
	scheduler();
}

void intervalTimerInterrupt_handler()
{
	LDIT(PSECOND);
	// "unblock all PCB's waiting a pseudo-clock" probably I have to call the ssi
	LDST((state_t *)BIOSDATAPAGE);
}
