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
static void send_message(state_t *p, msg_t *msg);
static void pass_up_or_die(int excp_value);

LIST_HEAD(blocked_on_receive);

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
		    (excCode >= 11 && excCode <= 24)) {
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
	if (proc_was_in_user_mode((pcb_t *)BIOSDATAPAGE)) {
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
			} else if (searchPcb(&pcbFree_h, ((pcb_t *)dest))) {
				((state_t *)BIOSDATAPAGE)->reg_a0 =
					DEST_NOT_EXIST;
			} else {
				msg_t *msg = allocMsg();
				msg->m_payload = payload;
				msg->m_sender = current_process;

				pcb_t *p = outProcQ(&blocked_on_receive,
						    ((pcb_t *)dest));
				if (p) {
					insertProcQ(&ready_queue,
						    ((pcb_t *)dest));
					if (was_pcb_soft_blocked(dest))
						softblock_count--;
					send_message(&dest->p_s, msg);

				} else {
					insertMessage(&dest->msg_inbox, msg);
				}
				((state_t *)BIOSDATAPAGE)->reg_a0 = 0;
			}
			((state_t *)BIOSDATAPAGE)->reg_sp += 4;
			LDST(((state_t *)BIOSDATAPAGE));

			break;
		}
		case RECEIVEMESSAGE: {
			pcb_t *sender =
				(pcb_t *)((state_t *)BIOSDATAPAGE)->reg_a1;
			msg_t *msg = popMessage(&((pcb_t *)sender)->msg_inbox,
						((pcb_t *)sender));

			if (msg) { //found message from sender
				if (was_pcb_soft_blocked(current_process))
					softblock_count--;
				send_message((state_t *)BIOSDATAPAGE, msg);
				LDST(((state_t *)BIOSDATAPAGE));
			} else { //no message found
				//the schedule will be called and all following code will not be executed
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

//the schedule will be called and all following code will not be executed
static void blockSys()
{
	insertProcQ(&blocked_on_receive, current_process);
	current_process->p_s = *((state_t *)BIOSDATAPAGE);
	if (should_pcb_be_soft_blocked(current_process))
		softblock_count++;
	current_process = NULL;
	//Aggiorno il CPU time TODO
	scheduler();
}

static void send_message(state_t *p, msg_t *msg)
{
	p->reg_a0 = (unsigned int)msg->m_sender;
	memaddr *payload_destination = (memaddr *)p->reg_a2;
	//save the payload in the address pointed by the reg_a2
	if (payload_destination) {
		*payload_destination = (unsigned int)msg->m_payload;
	}
	p->reg_sp += 4;
	freeMsg(msg);
}

void send_message_to_ssi(unsigned int payload)
{
	msg_t *msg = allocMsg();
	msg->m_payload = payload;
	msg->m_sender = (pcb_t *)INTERRUPT_HANDLER_MSG;
	pcb_t *p = outProcQ(&blocked_on_receive, ssi_pcb);
	if (p) {
		insertProcQ(&ready_queue, ssi_pcb);
		send_message(&ssi_pcb->p_s, msg);
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

static void pass_up_or_die(int excp_value) {
	if (current_process->p_supportStruct == NULL) {
		terminate_process(current_process);
	} else {
		current_process->p_supportStruct->sup_exceptState[excp_value] = *((state_t *)BIOSDATAPAGE);
		context_t *c = &current_process->p_supportStruct->sup_exceptContext[excp_value];
		LDCXT(c->stackPtr, c->status, c->pc);
		}
}