#include "headers/utils3.h"

extern pcb_t *ssi_pcb;

void p_term(pcb_t *arg)
{
	ssi_payload_t term_process_payload = {
		.service_code = TERMPROCESS,
		.arg = (void *)arg,
	};
	SYSCALL(SENDMESSAGE, (unsigned int)ssi_pcb,
		(unsigned int)(&term_process_payload), 0);
	SYSCALL(RECEIVEMESSAGE, (unsigned int)ssi_pcb, 0, 0);
	/*
	 * This code can't be reached. The Nucleus will terminate this process
	 * and the SYS2 is needed to wait for the ssi to terminate me.
	 * The Nucleus will take care of countinuing the execution.
	 */
}

pcb_PTR p_create(state_t *state, support_t *support)
{
	pcb_t *p;
	ssi_create_process_t ssi_create_process = {
		.state = state,
		.support = support,
	};
	ssi_payload_t payload = {
		.service_code = CREATEPROCESS,
		.arg = &ssi_create_process,
	};
	SYSCALL(SENDMESSAGE, (unsigned int)ssi_pcb, (unsigned int)&payload, 0);
	SYSCALL(RECEIVEMESSAGE, (unsigned int)ssi_pcb, (unsigned int)(&p), 0);
	return p;
}
