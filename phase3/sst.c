#include "headers/sst.h"
#include "headers/utils3.h"
#include <uriscv/liburiscv.h>

pcb_PTR sst_pcbs[UPROCMAX];

extern pcb_PTR test_pcb; // WE SHOULD NOT DO THIS. I think the cleanest way is
			 // to make syscalls on nucleus accept 0 as the parent
			 // sender
extern pcb_PTR ssi_pcb;

static void write(sst_print_t *write_payload, unsigned int asid,
		  unsigned int dev)
{
	// dev can only be IL_PRINTER or IL_TERMINAL
	switch (dev) {
	case IL_PRINTER:
		// print a string of caracters to the printer
		// with the same number of the sender ASID (-1)
		int count = 0;
		devreg_t *base = (devreg_t *)DEV_REG_ADDR(dev, asid - 1);
		unsigned int *data0 = &(base->dtp.data0);
		unsigned int *command = &(base->dtp.command);
		unsigned int status;

		while (write_payload->length > count) {
			unsigned int value =
				(unsigned int)*(write_payload->string + count);
			ssi_do_io_t do_io_char = {
				.commandAddr = data0,
				.commandValue = value,
			};
			ssi_payload_t payload_char = {
				.service_code = DOIO,
				.arg = &do_io_char,
			};

			// send char to printer
			SYSCALL(SENDMESSAGE, (unsigned int)ssi_pcb,
				(unsigned int)(&payload_char), 0);

			ssi_do_io_t do_io_command = {
				.commandAddr = command,
				.commandValue = PRINTCHR,
			};
			ssi_payload_t payload_command = {
				.service_code = DOIO,
				.arg = &do_io_command,
			};

			SYSCALL(SENDMESSAGE, (unsigned int)ssi_pcb,
				(unsigned int)(&payload_command), 0);
			SYSCALL(RECEIVEMESSAGE, (unsigned int)ssi_pcb,
				(unsigned int)(&status), 0);
			if ((status & WRITESTATUSMASK) != 1)
				PANIC();

			count++;
		}
		break;
	case IL_TERMINAL:
		// print a string of caracters to the terminal
		// with the same number of the sender ASID (-1)

		base = (devreg_t *)DEV_REG_ADDR(dev, asid - 1);
		command = &(base->term.transm_command);

		count = 0;
		while (write_payload->length > count) {
			unsigned int value =
				PRINTCHR |
				(((unsigned int)*(write_payload->string + count))
				 << 8);
			ssi_do_io_t do_io = {
				.commandAddr = command,
				.commandValue = value,
			};
			ssi_payload_t payload = {
				.service_code = DOIO,
				.arg = &do_io,
			};
			SYSCALL(SENDMESSAGE, (unsigned int)ssi_pcb,
				(unsigned int)(&payload), 0);
			SYSCALL(RECEIVEMESSAGE, (unsigned int)ssi_pcb,
				(unsigned int)(&status), 0);

			if ((status & WRITESTATUSMASK) != RECVD)
				PANIC();

			count++;
		}
		break;
	default:
		PANIC();
	}
}

void sst()
{
	// Get the support structure from the ssi. This is the same for the
	// child process
	ssi_payload_t getsupportdata = { .service_code = GETSUPPORTPTR,
					 .arg = NULL };
	SYSCALL(SENDMESSAGE, (unsigned int)ssi_pcb,
		(unsigned int)(&getsupportdata), 0);
	support_t *support;
	SYSCALL(RECEIVEMESSAGE, (unsigned int)ssi_pcb, (unsigned int)&support,
		0);

	// The asid variable is used to identify which u-proc the current sst
	// need to manage
	int asid = support->sup_asid;

	// SST creates a child process that executes one of the U-proc testers
	// SST shares the same ASID and support structure with its child U-proc.

	state_t child_state;
	STST(&child_state);

	child_state.pc_epc = 0x800000B0;
	child_state.reg_sp = 0xC0000000;

	// not sure about this, I took the values from p2test's processes
	child_state.status |= MSTATUS_MIE_MASK & ~MSTATUS_MPP_M;

	child_state.mie = MIE_ALL;
	pcb_PTR child_pcb = p_create(&child_state, support);

	// After its child initialization, the SST will wait for service
	// requests from its child process
	while (1) {
		ssi_payload_t *payload;
		SYSCALL(RECEIVEMESSAGE, (unsigned int)child_pcb,
			(unsigned int)&payload, 0);
		switch (payload->service_code) {
		case GET_TOD:
			unsigned int tod;
			STCK(tod);
			SYSCALL(SENDMESSAGE, (unsigned int)child_pcb, tod, 0);
			break;
		case TERMINATE:
			p_term(child_pcb);
			// TODO: DO NOT USE test_pcb
			SYSCALL(SENDMESSAGE, (unsigned int)test_pcb, 0, 0);
			p_term(SELF);
			break;
		case WRITEPRINTER:
			sst_print_t *print_payload =
				(sst_print_t *)payload->arg;
			write(print_payload, asid, IL_PRINTER);
			SYSCALL(SENDMESSAGE, (unsigned int)child_pcb, 0, 0);
			break;
		case WRITETERMINAL:
			sst_print_t *term_payload = (sst_print_t *)payload->arg;
			write(term_payload, asid, IL_TERMINAL);
			SYSCALL(SENDMESSAGE, (unsigned int)child_pcb, 0, 0);
			break;
		}
	}
}
