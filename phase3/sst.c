#include "headers/sst.h"
#include "headers/utils3.h"
#include <uriscv/liburiscv.h>

#define WRITESTATUSMASK 0xFF

pcb_PTR sst_pcbs[UPROCMAX];

extern pcb_PTR test_pcb; // WE SHOULD NOT DO THIS. I think the cleanest way is
			 // to make syscalls on nucleus accept 0 as the parent
			 // sender
extern pcb_PTR ssi_pcb;

static void write(sst_print_t *write_payload, unsigned int asid,
		  unsigned int dev)
{
	// dev can only be IL_PRINTER or IL_TERMINAL
	if (dev == IL_PRINTER) {
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
			ssi_do_io_t do_io = {
				.commandAddr = data0,
				.commandValue = value,
			};
			ssi_payload_t payload = {
				.service_code = DOIO,
				.arg = &do_io,
			};

			// send char to printer
			SYSCALL(SENDMESSAGE, (unsigned int)ssi_pcb,
				(unsigned int)(&payload), 0);
			SYSCALL(RECEIVEMESSAGE, (unsigned int)ssi_pcb,
				(unsigned int)(&status), 0);
			// 1 -> device ready, others are all errors
			if ((status & WRITESTATUSMASK) != 1)
				PANIC();

			// send command "print"
			do_io.commandAddr = command;
			do_io.commandValue = PRINTCHR;
			SYSCALL(SENDMESSAGE, (unsigned int)ssi_pcb,
				(unsigned int)(&payload), 0);
			SYSCALL(RECEIVEMESSAGE, (unsigned int)ssi_pcb,
				(unsigned int)(&status), 0);
			if ((status & WRITESTATUSMASK) != 1)
				PANIC();

			count++;
		}
	} else if (dev == IL_TERMINAL) {
		// print a string of caracters to the terminal
		// with the same number of the sender ASID (-1)

		devreg_t *base = (devreg_t *)DEV_REG_ADDR(dev, asid - 1);
		unsigned int *command = &(base->term.transm_command);
		unsigned int status;

		int count = 0;
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
	} else
		PANIC();
}

void sst()
{
	// The asid variable is used to identify which u-proc the current sst
	// need to manage
	unsigned int data;
	SYSCALL(RECEIVEMESSAGE, (unsigned int)test_pcb, (unsigned int)&data, 0);
	sst_child_t args = { .payload = data };

	int asid = GET_ASID;
	// SST creates a child process that executes one of the U-proc testers
	support_t *child_support = (support_t *)args.fields.support;
	child_support->sup_asid = asid;
	pcb_PTR child_pcb =
		p_create((state_t *)args.fields.state, child_support);

	// SST shares the same ASID and support structure with its child U-proc.
	// I think it is initProc.c job to give the sst process the same
	// asid and support structure as the child process

	// After its child initialization, the SST will wait for service
	// requests from its child process
	while (1) {
		unsigned int payload_tmp;
		SYSCALL(RECEIVEMESSAGE, (unsigned int)child_pcb,
			(unsigned int)&payload_tmp, 0);
		ssi_payload_t *payload = (ssi_payload_t *)payload_tmp;
		switch (payload->service_code) {
		case GET_TOD:
			SYSCALL(SENDMESSAGE, (unsigned int)child_pcb,
				child_pcb->p_time, 0);
			break;
		case TERMINATE:
			p_term(child_pcb);
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

	// TODO: DO NOT USE test_pcb
	SYSCALL(SENDMESSAGE, (unsigned int)test_pcb, 0, 0); // why?
	p_term(SELF);
}
