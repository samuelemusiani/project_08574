#include "headers/sysSupport.h"
#include "headers/sst.h"
#include "headers/utils3.h"
#include "headers/vmSupport.h"
#include "headers/initProc.h"
#include <uriscv/liburiscv.h>
#include <uriscv/types.h>

static void syscall_handler(support_t *support);

void general_exception_handler()
{
	ssi_payload_t getsupportdata = { .service_code = GETSUPPORTPTR,
					 .arg = NULL };
	SYSCALL(SENDMESSAGE, (unsigned int)ssi_pcb,
		(unsigned int)(&getsupportdata), 0);

	support_t *support;
	SYSCALL(RECEIVEMESSAGE, (unsigned int)ssi_pcb, (unsigned int)&support,
		0);

	state_t *s = &support->sup_exceptState[GENERALEXCEPT];
	unsigned int excCode = s->cause;

	if (excCode == 8) {
		syscall_handler(support);
	} else {
		trap_handler();
	}
}

void trap_handler()
{
	// terminate process
	mutex_payload_t p = { .fields.v = 1 };
	SYSCALL(SENDMESSAGE, (unsigned int)mutex_pcb, p.payload, 0);
	SYSCALL(SENDMESSAGE, PARENT, 0, 0);
	p_term(SELF);
}

static void syscall_handler(support_t *support)
{
	state_t *s = &support->sup_exceptState[GENERALEXCEPT];
	switch (s->reg_a0) {
	case SENDMSG: {
		pcb_t *dest = (pcb_t *)s->reg_a1;
		unsigned int payload = s->reg_a2;

		if (dest == PARENT) {
			unsigned int asid = support->sup_asid;
			dest = sst_pcbs[asid - 1];
		}

		s->reg_a0 =
			SYSCALL(SENDMESSAGE, (unsigned int)dest, payload, 0);

		break;
	}
	case RECEIVEMSG: {
		pcb_t *sender = (pcb_t *)s->reg_a1;

		if (sender == PARENT) {
			unsigned int asid = support->sup_asid;
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
	LDST(s);
}
