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
		if ((excCode >= 0 && excCode <= 7) ||
		    (excCode > 11 && excCode < 24)) {
			trap_handler();
		} else if (excCode >= 8 && excCode <= 11) {
			syscall_handler();
		} else if (excCode >= 24 && excCode <= 28) {
			tlb_handler();
		} else {
			PANIC(); // Lo facciamo ??
		}
	}
}

static void syscall_handler()
{
	if (proc_was_in_user_mode((state_t *)BIOSDATAPAGE)) {
		// Generate fake trap
		int trap_cause = 29;
		setCAUSE(trap_cause); //sys in usermode
		((state_t *)BIOSDATAPAGE)->cause = trap_cause;
		trap_handler();
	} else {
		pcb_t *dest = (pcb_t *)((state_t *)BIOSDATAPAGE)->reg_a1;
		unsigned int payload = ((state_t *)BIOSDATAPAGE)->reg_a2;

		switch (((state_t *)BIOSDATAPAGE)->reg_a0) {
		case SENDMESSAGE: {
			if (dest < pcbTable || dest > pcbTable + MAXPROC - 1) {
				((state_t *)BIOSDATAPAGE)->reg_a0 = MSGNOGOOD;
			} else if (searchPcb(&pcbFree_h, dest)) {
				((state_t *)BIOSDATAPAGE)->reg_a0 =
					DEST_NOT_EXIST;
			} else {
				msg_t *msg = allocMsg();
				msg->m_payload = payload;
				msg->m_sender = current_process;

				if (dest == ssi_pcb &&
				    is_a_softblocking_request(
					    (ssi_payload_t *)payload)) {
					current_process->do_io = 1;
				}

				if (!searchPcb(&ready_queue, dest) &&
				    dest != current_process) {
					// if dest is not in the ready queue,
					// it means that it is blocked on a receive message
					insertProcQ(&ready_queue,
						    ((pcb_t *)dest));

					if (dest->do_io) {
						dest->do_io = 0;
						softblock_count--;
					}

					deliver_message(&dest->p_s, msg);

				} else {
					insertMessage(&dest->msg_inbox, msg);
				}
				((state_t *)BIOSDATAPAGE)->reg_a0 = 0;
			}
			((state_t *)BIOSDATAPAGE)->pc_epc += 4;
			LDST(((state_t *)BIOSDATAPAGE));

			break;
		}
		case RECEIVEMESSAGE: {
			pcb_t *sender =
				(pcb_t *)((state_t *)BIOSDATAPAGE)->reg_a1;
			msg_t *msg = popMessage(&current_process->msg_inbox,
						((pcb_t *)sender));

			if (msg) { //found message from sender
				if (current_process->do_io) {
					current_process->do_io = 0;
				}
				deliver_message((state_t *)BIOSDATAPAGE, msg);
				LDST(((state_t *)BIOSDATAPAGE));
			} else { //no message found
				//the scheduler will be called and all following code will not be executed
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
	if (current_process->do_io)
		softblock_count++;
	current_process = NULL;
	//Aggiorno il CPU time TODO
	scheduler();
}

static void deliver_message(state_t *p, msg_t *msg)
{
	p->reg_a0 = (unsigned int)msg->m_sender;
	memaddr *payload_destination = (memaddr *)p->reg_a2;
	//save the payload in the address pointed by the reg_a2
	if (payload_destination) {
		*payload_destination = (unsigned int)msg->m_payload;
	}
	p->pc_epc += 4;
	freeMsg(msg);
}

void send_message_to_ssi(unsigned int payload)
{
	msg_t *msg = allocMsg();
	msg->m_payload = payload;
	msg->m_sender = (pcb_t *)INTERRUPT_HANDLER_MSG;
	if (!searchPcb(&ready_queue, ssi_pcb) && ssi_pcb != current_process) {
		insertProcQ(&ready_queue, ssi_pcb);
		deliver_message(&ssi_pcb->p_s, msg);
	} else {
		insertMessage(&ssi_pcb->msg_inbox, msg);
	}
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
	} else {
		current_process->p_supportStruct->sup_exceptState[excp_value] =
			*((state_t *)BIOSDATAPAGE);
		context_t *c = &current_process->p_supportStruct
					->sup_exceptContext[excp_value];
		LDCXT(c->stackPtr, c->status, c->pc);
	}
}
