#include "headers/initial.h"
#include "headers/exceptions.h"
#include "headers/scheduler.h"
#include "headers/ssi.h"

// The number of started, but not yet terminated processes
unsigned int process_count;

// The number of processes that are blocked
unsigned int softblock_count;

// Tail pointer to a queue of PCBs that are in the “ready” state.
struct list_head *ready_queue; // Tail pointer ?

// Pointer to the PCB that is in the “running” state
pcb_t *current_process;

struct list_head *pcb_blocked_on_device[MAXDEVICE]; // Array of queues
struct list_head *pcb_blocked_on_clock;

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
	INIT_LIST_HEAD(ready_queue);
	current_process = NULL;
	for (int i = 0; i < MAXDEVICE; i++) {
		INIT_LIST_HEAD(pcb_blocked_on_device[i]);
	}
	INIT_LIST_HEAD(pcb_blocked_on_clock);

	// Load the system-wide Interval Timer with 100 milliseconds (constant PSECOND)
	// We could move this in a public function. Other functions may need it
	*(memaddr *)INTERVALTMR = PSECOND;

	// Instantiate the first process (ssi)
	ssi_pcb = allocPcb();
	insertProcQ(ready_queue, ssi_pcb);
	process_count++;
	// In particular this process needs to have interrupts enabled
	ssi_pcb->p_s.mie = MIE_ALL; // This should enable all interrupts
	// kernel mode on
	ssi_pcb->p_s.status; //????
	// the SP set to RAMTOP (i.e. use the last RAM frame for its stack)
	RAMTOP(ssi_pcb->p_s.reg_sp); // ???
	// its PC set to the address of ssi() function.
	ssi_pcb->p_s.pc_epc = (memaddr)ssi; // ??
	// Set the remaining PCB fields
	// as follows:
	// - Set all the Process Tree fields to NULL.
	INIT_LIST_HEAD(&ssi_pcb->p_child);
	INIT_LIST_HEAD(&ssi_pcb->p_sib);
	// - Set the accumulated time field (p_time) to zero.
	ssi_pcb->p_time = 0;
	// - Set the Support Structure pointer (p_supportStruct) to NULL.
	ssi_pcb->p_supportStruct = NULL;
	// Important: When setting up a new processor state one must set the previous
	// bits (i.e. IEp & KUp) and not the current bits (i.e. IEc & KUc) in the
	// Status register for the desired assignment to take effect after the
	// initial LDST loads the processor state [Section 7.4-pops]. ????????

	// Instantiate the second process (test)
	pcb_t *test_pcb = allocPcb();
	insertProcQ(ready_queue, test_pcb);
	process_count++;
	// this process needs to have interrupts enabled, the processor Local Timer
	// enabled, kernel-mode on, the SP set to RAMTOP - (2 * FRAMESIZE) (i.e. use
	// the last RAM frame for its stack minus the space needed by the first
	// process), and its PC set to the address of test. Set the remaining PCB
	// fields as for the first proces
	// TODO

	// Call the scheduler
	scheduler();
	return 0;
}
