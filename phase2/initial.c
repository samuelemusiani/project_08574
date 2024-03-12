#include "headers/initial.h"
#include "headers/exceptions.h"
#include "headers/scheduler.h"
#include "headers/ssi.h"
#include <uriscv/liburiscv.h>
#include <uriscv/const.h>

// The number of started, but not yet terminated processes
unsigned int process_count;

// The number of processes that are blocked on an I/O request
unsigned int softblock_count;

// Store the tod_timer
cpu_t tod_timer;

// Tail pointer to a queue of PCBs that are in the “ready” state.
struct list_head ready_queue; // Tail pointer ?

// Pointer to the PCB that is in the “running” state
pcb_t *current_process;

struct list_head pcb_blocked_on_device[MAXDEVICE]; // Array of queues
struct list_head pcb_blocked_on_clock;

// Blocked PCBs: The Nucleus maintains one list of blocked PCBs for each
// external (sub)device, plus one additional list to support the Pseudo-clock
// [Section 8.3]. Since terminal devices are actually two independent
// sub-devices, the Nucleus maintains two lists/pointers for each terminal
// device [Section 5.7- pops].

pcb_t *ssi_pcb;
pcb_t *ssi_pcb_real;

int main(void)
{
	// Populate the Process 0 Pass Up Vector
	passupvector_t *passupvector = (passupvector_t *)PASSUPVECTOR;
	passupvector->tlb_refill_handler = (memaddr)uTLB_RefillHandler;
	passupvector->tlb_refill_stackPtr = (memaddr)KERNELSTACK;
	passupvector->exception_handler = (memaddr)exception_handler;
	passupvector->exception_stackPtr = (memaddr)KERNELSTACK;

	// Initialize the Level 2 (Phase 1) data structures

	initPcbs();
	initMsgs();

	// Initialize all the previously declared variables;
	process_count = 0;
	softblock_count = 0;
	INIT_LIST_HEAD(&ready_queue);
	current_process = NULL;
	for (int i = 0; i < MAXDEVICE; i++) {
		INIT_LIST_HEAD(&pcb_blocked_on_device[i]);
	}
	INIT_LIST_HEAD(&pcb_blocked_on_clock);

	// Load the system-wide Interval Timer with 100 milliseconds
	LDIT(PSECOND);

	// Instantiate the first process (ssi)
	ssi_pcb_real = allocPcb();
	insertProcQ(&ready_queue, ssi_pcb_real);
	process_count++;
	// In particular, this process needs to have interrupts enabled and
	// kernel mode
	ssi_pcb_real->p_s.status = KERNELMODE | INTERRUPTS_ENBALED;
	ssi_pcb_real->p_s.mie = MIE_ALL;
	// the SP set to RAMTOP (i.e., use the last RAM frame for its stack)
	RAMTOP(ssi_pcb_real->p_s.reg_sp);
	// PC set to the address of ssi() function.
	ssi_pcb_real->p_s.pc_epc = (memaddr)ssi;

	ssi_pcb = SSI_FAKE_ADDR;

	// Instantiate the second process (test)
	pcb_t *test_pcb = allocPcb();
	insertProcQ(&ready_queue, test_pcb);
	process_count++;
	// This process needs to have interrupts enabled and kernel-mode on
	test_pcb->p_s.status = INTERRUPTS_ENBALED | KERNELMODE;
	test_pcb->p_s.mie = MIE_ALL;
	// the SP set to RAMTOP - (2 * FRAMESIZE)
	memaddr ramtop;
	RAMTOP(ramtop);
	unsigned int framesize = 0x00001000;
	test_pcb->p_s.reg_sp = ramtop - (2 * framesize);
	// PC set to the address of test.
	test_pcb->p_s.pc_epc = (memaddr)test;

	// Call the scheduler
	scheduler();

	return 0;
}
