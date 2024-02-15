#include "headers/initial.h"
#include "headers/exceptions.h"
#include "headers/scheduler.h"
#include "headers/ssi.h"
#include <uriscv/liburiscv.h>
#include <uriscv/const.h>

// The number of started, but not yet terminated processes
unsigned int process_count;

// The number of processes that are blocked
unsigned int softblock_count;

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

pcb_t *ssi_pcb; // This is needed as p2test expects it :(

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
	// I don't think we need to do this as they are initialized on declaration
	process_count = 0;
	softblock_count = 0;
	INIT_LIST_HEAD(&ready_queue);
	current_process = NULL;
	for (int i = 0; i < MAXDEVICE; i++) {
		INIT_LIST_HEAD(&pcb_blocked_on_device[i]);
	}
	INIT_LIST_HEAD(&pcb_blocked_on_clock);

	// Load the system-wide Interval Timer with 100 milliseconds (constant PSECOND)
	// We could move this in a public function. Other functions may need it
	LDIT(PSECOND);

	// Instantiate the first process (ssi)
	ssi_pcb = allocPcb();
	insertProcQ(&ready_queue, ssi_pcb);
	process_count++;
	// In particular this process needs to have interrupts enabled, kernel mode on
	ssi_pcb->p_s.status = KERNELMODE | INTERRUPTS_ENBALED;
	ssi_pcb->p_s.mie = MIE_ALL;
	// the SP set to RAMTOP (i.e. use the last RAM frame for its stack)
	RAMTOP(ssi_pcb->p_s.reg_sp); // ??
	// its PC set to the address of ssi() function.
	ssi_pcb->p_s.pc_epc = (memaddr)ssi;
	// Set the remaining PCB fields as follows:
	//
	// - Set all the Process Tree fields to NULL.
	// INIT_LIST_HEAD(&ssi_pcb->p_child); //Should not be needed as alloPcb does it
	// INIT_LIST_HEAD(&ssi_pcb->p_sib); // Should not be needed as alloPcb does it
	//
	// - Set the accumulated time field (p_time) to zero.
	// ssi_pcb->p_time = 0; // Should not be needed as alloPcb does it
	//
	// - Set the Support Structure pointer (p_supportStruct) to NULL.
	// ssi_pcb->p_supportStruct = NULL; // Should not be needed as alloPcb does it

	// Instantiate the second process (test)
	pcb_t *test_pcb = allocPcb();
	insertProcQ(&ready_queue, test_pcb);
	process_count++;
	// this process needs to have interrupts enabled, the processor Local Timer
	// enabled, kernel-mode on,
	test_pcb->p_s.status = INTERRUPTS_ENBALED | KERNELMODE;
	test_pcb->p_s.mie = MIE_ALL;
	// the SP set to RAMTOP - (2 * FRAMESIZE) (i.e. use the last RAM frame for
	// its stack minus the space needed by the first process),
	memaddr ramtop;
	RAMTOP(ramtop);
	unsigned int framesize = 0x00001000; // TODO: Find the real value
	test_pcb->p_s.reg_sp = ramtop - (2 * framesize);
	// and its PC set to the address of test.
	test_pcb->p_s.pc_epc = (memaddr)test;
	// Set the remaining PCB fields as for the first process
	// INIT_LIST_HEAD(&test_pcb->p_child);
	// INIT_LIST_HEAD(&test_pcb->p_sib);
	// test_pcb->p_time = 0;
	// test_pcb->p_supportStruct = NULL;

	// Call the scheduler
	scheduler();

	return 0;
}
