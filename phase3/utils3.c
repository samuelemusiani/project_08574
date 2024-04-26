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
}
