#include "headers/exceptions.h"
#include "headers/initial.h"
#include "headers/interrupts.h"
#include "headers/utils.h"
#include "headers/scheduler.h"
#include "headers/ssi.h"

#include <uriscv/liburiscv.h>
#include <uriscv/types.h>
#include <uriscv/cpu.h>

static void trap_handler();
static void syscall_handler();
static void tlb_handler();
static void blockSys();
static void deliver_message(state_t *p, msg_t *msg);
static void pass_up_or_die(int excp_value);
static unsigned int send_message(pcb_t *dest, unsigned int payload,
				 pcb_t *sender);

static int is_waiting_for_me(pcb_t *sender, pcb_t *dest);

void uTLB_RefillHandler()
{
	setENTRYHI(0x80000000);
	setENTRYLO(0x00000000);
	TLBWR();
	LDST((state_t *)0x0FFFF000);
}

void exception_handler()
{
	unsigned int mcause = getCAUSE();
	if (CAUSE_IS_INT(mcause)) { // Is interrupt
		interrupt_handler();
	} else {
		unsigned int excCode = mcause & GETEXECCODE;
		if ((excCode <= 7) || (excCode > 11 && excCode < 24)) {
			trap_handler();
		} else if (excCode <= 11) {
			syscall_handler();
		} else if (excCode <= 28) {
			tlb_handler();
		} else {
			PANIC(); // If we could not handle the exception, we panic
		}
	}
}

static void syscall_handler()
{
	if ((getCAUSE() & GETEXECCODE) == 8) {
		// On uMPS3 we should generate fake trap with a reserverd code of PRIVINSTR
		// that has value 10. In uRISCV this is not needed as a paricular code is
		// used for SYS in user mode (8).
		trap_handler();
	} else {
		switch (((state_t *)BIOSDATAPAGE)->reg_a0) {
		case SENDMESSAGE: {
			pcb_t *dest =
				(pcb_t *)((state_t *)BIOSDATAPAGE)->reg_a1;
			unsigned int payload =
				((state_t *)BIOSDATAPAGE)->reg_a2;

			unsigned int return_val = DEST_NOT_EXIST;

			if (isPcbValid(dest)) {
				return_val = send_message(dest, payload,
							  current_process);
			}
			// Set return value
			((state_t *)BIOSDATAPAGE)->reg_a0 = return_val;

			// Increment PC to avoid sys loop
			((state_t *)BIOSDATAPAGE)->pc_epc += 4;

			LDST(((state_t *)BIOSDATAPAGE));

			break;
		}
		case RECEIVEMESSAGE: {
			pcb_t *sender =
				(pcb_t *)((state_t *)BIOSDATAPAGE)->reg_a1;
			msg_t *msg = popMessage(&current_process->msg_inbox,
						((pcb_t *)sender));

			if (msg) { // Found message from sender
				if (current_process->do_io) {
					current_process->do_io = 0;
				}
				deliver_message((state_t *)BIOSDATAPAGE, msg);
				LDST(((state_t *)BIOSDATAPAGE));
			} else {
				// The scheduler will be called and all following code will not
				// be executed
				blockSys();
			}
			break;
		}
		default:
			pass_up_or_die(GENERALEXCEPT);
			break;
		}
	}
}

static void blockSys()
{
	current_process->p_s = *((state_t *)BIOSDATAPAGE);
	update_cpu_time();
	if (current_process->do_io)
		softblock_count++;
	current_process = NULL;
	scheduler();
}

static void deliver_message(state_t *p, msg_t *msg)
{
	p->reg_a0 = (unsigned int)msg->m_sender;
	//save the payload in the address pointed by the reg_a2
	memaddr *payload_destination = (memaddr *)p->reg_a2;
	if (payload_destination) {
		*payload_destination = (unsigned int)msg->m_payload;
	}
	p->pc_epc += 4;
	freeMsg(msg);
}

static int is_waiting_for_me(pcb_t *sender, pcb_t *dest)
{
	return dest->p_s.reg_a1 == (unsigned int)sender ||
	       dest->p_s.reg_a1 == ANYMESSAGE;
}

static unsigned int send_message(pcb_t *dest, unsigned int payload,
				 pcb_t *sender)
{
	msg_t *msg = allocMsg();
	if (!msg)
		return MSGNOGOOD;

	msg->m_payload = payload;
	msg->m_sender = sender;

	// We need to check if the sender is the interrupt_handler before the
	// evaluation of is_a_softblocking_request() because we can't dereference the
	// payload if sent by the interrupt_handler.
	if (dest == ssi_pcb && sender != (pcb_t *)INTERRUPT_HANDLER_MSG &&
	    is_a_softblocking_request((ssi_payload_t *)payload)) {
		current_process->do_io = 1;
	}

	if (!searchPcb(&ready_queue, dest) && dest != current_process &&
	    is_waiting_for_me(sender, dest)) {
		// if dest is not in the ready queue,
		// it means that it is blocked on a receive message
		insertProcQ(&ready_queue, ((pcb_t *)dest));

		if (dest->do_io) {
			dest->do_io = 0;
			softblock_count--;
		}

		deliver_message(&dest->p_s, msg);

	} else {
		insertMessage(&dest->msg_inbox, msg);
	}

	// Set return value for sys SENDMSG
	return 0;
}

void send_message_to_ssi(unsigned int payload)
{
	send_message(ssi_pcb, payload, (pcb_t *)INTERRUPT_HANDLER_MSG);
}

static void trap_handler()
{
	pass_up_or_die(GENERALEXCEPT);
}

static void tlb_handler()
{
	pass_up_or_die(PGFAULTEXCEPT);
}

static void pass_up_or_die(int excp_value)
{
	if (current_process->p_supportStruct == NULL) {
		terminate_process(current_process);
		scheduler();
	} else {
		current_process->p_supportStruct->sup_exceptState[excp_value] =
			*((state_t *)BIOSDATAPAGE);
		context_t *c = &current_process->p_supportStruct
					->sup_exceptContext[excp_value];
		LDCXT(c->stackPtr, c->status, c->pc);
	}
}
