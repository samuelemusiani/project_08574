#include "headers/initProc.h"
#include "headers/vmSupport.h"
#include "headers/sysSupport.h"
#include "headers/sst.h"
#include "headers/utils3.h"
#include "headers/support.h"

pcb_PTR mutex_pcb;

memaddr stacks[UPROCMAX * 2];

void test()
{
	initSwapStructs();
	initSupports();

	state_t tmp;
	STST(&tmp);

	unsigned int stack_ptr = tmp.reg_sp;

	for (int i = 0; i < UPROCMAX * 2; i++) {
		stack_ptr -= (2 * QPAGE);
		stacks[i] = stack_ptr;
	}

	// Init mutex process
	state_t mutexstate = tmp;
	stack_ptr -= QPAGE;
	mutexstate.reg_sp = stack_ptr;
	mutexstate.pc_epc = (memaddr)mutex_proc;
	mutexstate.status = MSTATUS_MPP_M | MSTATUS_MPIE_MASK;
	mutexstate.mie = MIE_ALL;
	mutex_pcb = p_create(&mutexstate, NULL);

	// TODO: Launch a proc for every I/O device. This is optional and we can
	// do it later

	// Create 8 sst
	for (int i = 1; i <= UPROCMAX; i++) {
		state_t tmpstate;
		STST(&tmpstate);
		tmpstate.entry_hi = i << ASIDSHIFT;
		stack_ptr -= QPAGE;
		tmpstate.reg_sp = stack_ptr;
		tmpstate.pc_epc = (memaddr)sst;
		tmpstate.status |= MSTATUS_MPP_M | MSTATUS_MPIE_MASK;
		tmpstate.mie = MIE_ALL;

		support_t *s = allocSupport();

		s->sup_asid = i;

		// PGFAULTEXCEPT
		context_t tmp = { // clang-format off
      .stackPtr = stacks[i - 1],
			.status = MSTATUS_MPP_M | MSTATUS_MPIE_MASK | MSTATUS_MIE_MASK,
			.pc = (memaddr)tlb_handler 
    }; // clang-format on 
		s->sup_exceptContext[PGFAULTEXCEPT] = tmp;

		// GENERALEXCEPT
		tmp.stackPtr = stacks[UPROCMAX + i - 1];
		tmp.pc = (memaddr)general_exception_handler;
		s->sup_exceptContext[GENERALEXCEPT] = tmp;

		s->sup_asid = i;

		sst_pcbs[i - 1] = p_create(&tmpstate, s);
	}

	for (int i = 0; i < UPROCMAX; i++) {
		pcb_t *s = (pcb_t *)SYSCALL(RECEIVEMESSAGE, ANYMESSAGE, 0, 0);
		mark_free_pages(s->p_supportStruct->sup_asid);
	}

	p_term(mutex_pcb);
	p_term(SELF);
}
