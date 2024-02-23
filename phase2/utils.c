#include "headers/utils.h"
#include "headers/initial.h"

#include <uriscv/arch.h>

void terminate_process(pcb_t *p)
{
	outChild(p);

	int was_waiting = 0;

	// If waiting for IO we should remove him for the queue
	for (int i = 0; i < MAXDEVICE; i++) {
		// TODO: Optimize, I can wait only on one device
		if (outProcQ(&pcb_blocked_on_device[i], p) == p)
			was_waiting = 1;
	}

	// If waiting for clock we should remove him for the queue
	if (outProcQ(&pcb_blocked_on_clock, p) == p)
		was_waiting = 1;

	process_count--;
	softblock_count -= was_waiting;

	// We should terminate all his progenesis
	while (!emptyChild(p)) {
		terminate_process(headProcQ(&p->p_child));
	}

	freePcb(p);
}

int proc_was_in_kernel_mode(pcb_t *p)
{
	return p->p_s.status & MSTATUS_MIE_MASK;
}

int proc_was_in_user_mode(pcb_t *p)
{
	return !proc_was_in_kernel_mode(p);
}

// From gcc/libgcc/memcpy.c
void *memcpy(void *dest, const void *src, unsigned int len)
{
	char *d = dest;
	const char *s = src;
	while (len--)
		*d++ = *s++;
	return dest;
}

int device_number_from_type_il(int type, int il)
{
	return type + il * N_DEV_PER_IL;
}

// This functions maps a memaddr to a number in [0,40] in order to map the
// command for a device to a position in the pcb_blocked_on_device[] array.
int comm_add_to_number(memaddr command_addr)
{
	int tmp = ((command_addr - (command_addr % 4)) - DEV_REG_START) /
		  DEV_REG_SIZE;
	int interrupt_line_number = tmp / 8; // Start from zero
	int dev_number = tmp - interrupt_line_number * N_DEV_PER_IL;

	return device_number_from_type_il(dev_number, interrupt_line_number);
}
