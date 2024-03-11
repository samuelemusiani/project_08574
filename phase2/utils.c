#include "headers/utils.h"
#include "headers/initial.h"

#include <uriscv/arch.h>

void terminate_process(pcb_t *p)
{
	if (p == ssi_pcb)
		PANIC();

	outChild(p);
	outProcQ(&ready_queue, p);
	outProcQForIO(&ready_queue, p);

	process_count--;
	// There is a small case in which the process have do_io set to 1 but is
	// not waiting yet (between a SEND and a RECEIVE). Because it is not
	// waiting, the softblock_count is not really incremented. We should
	// check if this process has already entered the blocked state.
	softblock_count -= p->do_io;

	// We should terminate all his progenesis
	while (!emptyChild(p)) {
		terminate_process(container_of(p->p_child.next, pcb_t, p_sib));
	}

	freePcb(p);
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

// type = {0..4}
// number = {0..7}
int hash_from_device_type_number(int type, int number)
{
	return type * N_DEV_PER_IL + number;
}

// This functions maps a memaddr to a number in [0,40] in order to map the
// command for a device to a position in the pcb_blocked_on_device[] array.
int comm_add_to_number(memaddr command_addr)
{
	int tmp = ((command_addr - (command_addr % 4)) - DEV_REG_START) /
		  DEV_REG_SIZE;
	int interrupt_line_number = tmp / 8; // Start from zero
	int dev_number = tmp - interrupt_line_number * N_DEV_PER_IL;

	return hash_from_device_type_number(interrupt_line_number, dev_number);
}

void update_cpu_time()
{
	cpu_t current_TOD;
	STCK(current_TOD);
	if (current_process != NULL) {
		current_process->p_time += current_TOD - tod_timer;
	}
}
