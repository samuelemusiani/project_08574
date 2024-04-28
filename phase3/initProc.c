#include "headers/initProc.h"
#include "headers/vmSupport.h"
#include "headers/sst.h"
#include "headers/utils3.h"

#define QPAGE 1024

pcb_PTR mutex_pcb, sst1;

void test()
{
	initSwapStructs();

	// Init mutex process
	state_t mutexstate;
	STST(&mutexstate);
	mutexstate.reg_sp = mutexstate.reg_sp - QPAGE; // ???
	// The mutex is for the Swap pool table, so if the mutex process has
	// virtual memory there could be some conflicts.
	// We put the mutex process under the RAMTOP limit to avoid VM.
	// QPAGE may be too small?
	mutexstate.pc_epc = (memaddr)mutex_proc;
	mutexstate.status = MSTATUS_MPP_M; // ??? Forse or |=? Interrupt
					   // abilitati?

	// mutexstate.mie = MIE_ALL;
	mutex_pcb = p_create(&mutexstate, NULL);

	// TODO: Launch a proc for every I/O device. This is optional and we can
	// do it later

	// Create 8 sst
	for (int i = 1; i <= 1 /* 8 */; i++) {
		state_t tmpstate;
		STST(&tmpstate);
		tmpstate.reg_sp = mutexstate.reg_sp - QPAGE * i; // ??
		tmpstate.pc_epc = (memaddr)sst;
		tmpstate.status |= MSTATUS_MPP_M | MSTATUS_MIE_BIT; // ???
		// In order to use SYSCALLS, SST need to be in kernel mode (?)
		tmpstate.mie = MIE_ALL;
		tmpstate.entry_hi |= i << ASIDSHIFT;

		/* ??? */
		state_t *child_state;
		support_t *support; // sia per sst che per processo figlio

		// se gli ASID sono [1..8], gli indici di sst_pcbs possono
		// essere ASID-1 ?
		sst_pcbs[i - 1] = p_create(&tmpstate, support);
		sst_child_t data = { .fields.state = (unsigned int)child_state,
				     (unsigned int)support };

		SYSCALL(SENDMESSAGE, (unsigned int)sst_pcbs[i - 1],
			(unsigned int)&data, 0);
	}

	// Other 7...

	// Wait for sst messages when terminated
	for (int i = 0; i < 1 /* 8 */; i++) {
		SYSCALL(RECEIVEMESSAGE, ANYMESSAGE, 0, 0);
	}
	p_term(SELF);
}
