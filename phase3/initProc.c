#include "headers/initProc.h"
#include "headers/vmSupport.h"
#include "headers/sysSupport.h"
#include "headers/sst.h"
#include "headers/utils3.h"

pcb_PTR mutex_pcb, sst1;
support_t support_table[UPROCMAX];

void test()
{
	initSwapStructs();

	// Init mutex process
	state_t mutexstate;
	STST(&mutexstate);
	mutexstate.reg_sp = mutexstate.reg_sp - QPAGE; // ???
	// The mutex is for the Swap pool table, so if the mutex process has
	// virtual memory, there could be some conflicts.
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

		support_table[i - 1].sup_asid = i;
		// PGFAULTEXCEPT
		context_t tmp = { .stackPtr =
					  (unsigned int)&support_table[i - 1]
						  .sup_stackTLB[499],
				  .status = MSTATUS_MPP_M | MSTATUS_MIE_BIT,
				  .pc = (memaddr)tlb_handler };
		support_table[i - 1].sup_exceptContext[PGFAULTEXCEPT] = tmp;

		// GENERALEXCEPT
		tmp.stackPtr =
			(unsigned int)&support_table[i - 1].sup_stackGen[499];
		tmp.pc = (memaddr)general_exception_handler;
		support_table[i - 1].sup_exceptContext[GENERALEXCEPT] = tmp;

		support_table[i - 1].sup_asid = i;

		sst_pcbs[i - 1] = p_create(&tmpstate, &support_table[i - 1]);
	}

	// Other 7...

	// Wait for sst messages when terminated
	for (int i = 0; i < 1 /* 8 */; i++) {
		SYSCALL(RECEIVEMESSAGE, ANYMESSAGE, 0, 0);
	}

	p_term(mutex_pcb);
	p_term(SELF);
}
