#include "headers/utils.h"
#include "headers/initial.h"

#include <uriscv/arch.h>

/*
 * This function terminates the specified process and all its progeny. It
 * removes the process from the ready queue, updates the process count and soft
 * block count, and frees the process control block (PCB) memory.
 */
void terminate_process(pcb_t *p)
{
	if (p == ssi_pcb_real)
		PANIC();

	if (!isPcbValid(p))
		return;

	outChild(p);
	outProcQ(&ready_queue, p);

	int was_blocked = 0;
	for (int i = 0; i < MAXDEVICE; i++) {
		if (outProcQForIO(&pcb_blocked_on_device[i], p) == p)
			was_blocked = 1;
	}
	if (outProcQ(&pcb_blocked_on_clock, p) == p)
		was_blocked = 1;

	process_count--;

	/*
	 * There is a small case in which the process have do_io set to 1 but is
	 * not waiting yet (between a SEND and a RECEIVE). Because it is not
	 * waiting, the softblock_count is not really incremented.
	 */
	if (was_blocked)
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

/*
 * Processes blocked for IO are stored in an array of queues.
 * This function is used to map a device type and a device number to a number in
 * [0, MAXDEVICE] in order to store the PCB in the pcb_blocked_on_device[]
 * array.
 * Device type: type = [0..4]
 * Interrupt line: number = [0..7]
 *
 * Variable transm is used when the device is a terminal. If 1, it indicates
 * that the subterminal is the transmittion one.
 */
int hash_from_device_type_number(int type, int number, int transm)
{
	if (type == EXT_IL_INDEX(IL_TERMINAL))
		type += !!transm;

	return type * N_DEV_PER_IL + number;
}

/*
 * This functions maps a memaddr to a number in [0,MAXDEVICE] in order to map
 * the command for a device to a position in the pcb_blocked_on_device[] array.
 */
int comm_add_to_number(memaddr command_addr)
{
	unsigned int tmp1 = (command_addr - DEV_REG_START) / DEV_REG_SIZE;
	unsigned int il_number = tmp1 / N_DEV_PER_IL;
	unsigned int dev_number = tmp1 - (il_number * N_DEV_PER_IL);

	int n = hash_from_device_type_number(il_number, dev_number, 1);
	return n;
}

// This function is used to update the cpu time of the current process
void update_cpu_time()
{
	cpu_t current_TOD;
	STCK(current_TOD);
	if (current_process != NULL) {
		current_process->p_time += current_TOD - tod_timer;
	}
}
