#include "headers/initProc.h"
#include "headers/vmSupport.h"
#include "headers/sysSupport.h"
#include "headers/sst.h"
#include "headers/utils3.h"
#include "headers/support.h"

pcb_PTR mutex_pcb;

void test()
{
	initSwapStructs();
	initSupports();

	// Init mutex process
	state_t mutexstate;
	STST(&mutexstate);
	mutexstate.reg_sp = mutexstate.reg_sp - QPAGE;
	mutexstate.pc_epc = (memaddr)mutex_proc;
	mutexstate.status = MSTATUS_MPP_M;
	mutexstate.mie = MIE_ALL;
	mutex_pcb = p_create(&mutexstate, NULL);

	// TODO: Launch a proc for every I/O device. This is optional and we can
	// do it later

	// Create 8 sst
	for (int i = 1; i <= 8; i++) {
		state_t tmpstate;
		STST(&tmpstate);
		tmpstate.entry_hi = i << ASIDSHIFT;
		tmpstate.reg_sp = mutexstate.reg_sp - QPAGE * i;
		tmpstate.pc_epc = (memaddr)sst;
		tmpstate.status |= MSTATUS_MPP_M | MSTATUS_MPIE_MASK |
				   MSTATUS_MIE_MASK;
		tmpstate.mie = MIE_ALL;

		support_t *s = allocSupport();

		s->sup_asid = i;
		// PGFAULTEXCEPT
		context_t tmp = { .stackPtr =
					  (unsigned int)&s->sup_stackTLB[499],
				  .status = MSTATUS_MPP_M | MSTATUS_MPIE_MASK |
					    MSTATUS_MIE_MASK,
				  .pc = (memaddr)tlb_handler };
		s->sup_exceptContext[PGFAULTEXCEPT] = tmp;

		// GENERALEXCEPT
		tmp.stackPtr = (unsigned int)&s->sup_stackGen[499];
		tmp.pc = (memaddr)general_exception_handler;
		s->sup_exceptContext[GENERALEXCEPT] = tmp;

		s->sup_asid = i;

		sst_pcbs[i - 1] = p_create(&tmpstate, s);
	}

	for (int i = 0; i < 8; i++) {
		pcb_t *s = (pcb_t *)SYSCALL(RECEIVEMESSAGE, ANYMESSAGE, 0, 0);
		mark_free_pages(s->p_supportStruct->sup_asid);
	}

	p_term(mutex_pcb);
	p_term(SELF);
}
