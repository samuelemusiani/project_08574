#include "headers/initProc.h"
#include "headers/sst.h"
#include <uriscv/liburiscv.h>

#define QPAGE 1024
#define SELF NULL

#define SWAPPOOLDIM (2 * UPROCMAX)

extern pcb_t *ssi_pcb;

pcb_PTR mutex_pcb, sst1;

swap_t *swap_pool;

static pcb_t *create_process(state_t *s)
{
	pcb_t *p;
	ssi_create_process_t ssi_create_process = {
		.state = s,
		.support = NULL,
	};
	ssi_payload_t payload = {
		.service_code = CREATEPROCESS,
		.arg = &ssi_create_process,
	};
	SYSCALL(SENDMESSAGE, (unsigned int)ssi_pcb, (unsigned int)&payload, 0);
	SYSCALL(RECEIVEMESSAGE, (unsigned int)ssi_pcb, (unsigned int)(&p), 0);
	return p;
}

static void terminate_process(pcb_t *arg)
{
	ssi_payload_t term_process_payload = {
		.service_code = TERMPROCESS,
		.arg = (void *)arg,
	};
	SYSCALL(SENDMESSAGE, (unsigned int)ssi_pcb,
		(unsigned int)(&term_process_payload), 0);
	SYSCALL(RECEIVEMESSAGE, (unsigned int)ssi_pcb, 0, 0);
}

// Processo mutex
static void mutex_proc()
{
	// TODO: Mutex process
	terminate_process(SELF);
}

void test()
{
	// Init swap pool table
	swap_pool = (swap_t *)FLASHPOOLSTART;

	for (int i = 0; i < SWAPPOOLDIM; i++) {
		swap_pool[i].sw_asid = -1;
	}

	// Init mutex process
	state_t mutexstate;
	STST(&mutexstate);
	mutexstate.reg_sp = mutexstate.reg_sp - QPAGE; // ???
	mutexstate.pc_epc = (memaddr)mutex_proc;
	mutexstate.status = MSTATUS_MPP_M; // ??? Forse or |=? Interrupt
					   // abilitati?

	// mutexstate.mie = MIE_ALL;

	// Create 8 sst
	state_t sst1state;
	STST(&sst1state);
	sst1state.reg_sp = mutexstate.reg_sp - QPAGE; // ??
	sst1state.pc_epc = (memaddr)sst;
	sst1state.status |= MSTATUS_MPP_M | MSTATUS_MIE_BIT; // ???
	sst1state.mie = MIE_ALL;

	// Other 7...

	mutex_pcb = create_process(&mutexstate);
	terminate_process(SELF);
}
