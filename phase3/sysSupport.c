#include "headers/sysSupport.h"
#include "headers/sst.h"
#include "headers/utils3.h"
#include <uriscv/liburiscv.h>
#include <uriscv/types.h>

static void syscall_handler(state_t *s);
void trap_handler(state_t *s);

void general_exception_handler()
{
	// get the old state from the support level struct

	SYSCALL(SENDMESSAGE, (unsigned int)ssi_pcb, GETSUPPORTPTR, 0);
	support_t *support;
	SYSCALL(RECEIVEMESSAGE, (unsigned int)ssi_pcb, (unsigned int)&support,
		0); // TODO: Check return code

	state_t *s = &support->sup_exceptState[GENERALEXCEPT];
	unsigned int excCode = s->cause;

	if (excCode == 8) {
		syscall_handler(s);
	} else {
		trap_handler(s);
	}
}

void trap_handler(state_t *s)
{
	// TODO Release of the mutual exclusion on the SWAP Pool

	// terminate process
	p_term(SELF);
}

static void syscall_handler(state_t *s)
{
	switch (s->reg_a0) {
	case SENDMSG: {
		pcb_t *dest = (pcb_t *)s->reg_a1;
		unsigned int payload = s->reg_a2;

		// PARENT == 0
		if (!dest) {
			unsigned int asid = GET_ASID;
			dest = sst_pcbs[asid - 1];
		}

		s->reg_a0 =
			SYSCALL(SENDMESSAGE, (unsigned int)dest, payload, 0);

		break;
	}
	case RECEIVEMSG: {
		pcb_t *sender = (pcb_t *)s->reg_a1;

		// PARENT == 0
		if (!sender) {
			unsigned int asid = GET_ASID;
			sender = sst_pcbs[asid - 1];
		}

		s->reg_a0 = SYSCALL(RECEIVEMESSAGE, (unsigned int)sender,
				    s->reg_a2, 0);

		break;
	}
	default:
		PANIC();
		break;
	}
	s->pc_epc += 4;
	LDST(s); // TODO: Should we really do this?
}
