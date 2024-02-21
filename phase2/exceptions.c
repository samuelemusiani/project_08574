#include "headers/exceptions.h"
#include "headers/initial.h"
#include "headers/interrupts.h"
#include "headers/utils.h"
#include "headers/scheduler.h"
#include <uriscv/liburiscv.h>
#include <uriscv/types.h>
#include <uriscv/cpu.h>

static void trap_handler();
static void syscall_handler();
static void tlb_handler();

struct list_head unanswered_receive;

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
	if (proc_was_in_user_mode((state_t *)BIOSDATAPAGE)) {
		// Generate fake interrupt
	} else {
		unsigned int dest = ((state_t *)BIOSDATAPAGE)->reg_a1;
		unsigned int msg = ((state_t *)BIOSDATAPAGE)->reg_a2;
		switch (((state_t *)BIOSDATAPAGE)->reg_a0)
		{
		case SENDMESSAGE: {
			if(searchPcb(&pcbFree_h, ((pcb_t *)dest))) {
				((state_t *)BIOSDATAPAGE)->reg_a0 = DEST_NOT_EXIST;
			} else {
				if(searchPcb(&ready_queue, ((pcb_t *)dest))) { // cerco nella ready queue il processo che mi interessa
				//devo modificare il registro inbox del pcb a1 (destination) con il payload a2
				insertMessage(&((pcb_t *)dest)->msg_inbox, (msg_t*) msg);

				} else if (searchPcb(&unanswered_receive, ((pcb_t *)BIOSDATAPAGE))) {//cerco l'elemento nella coda dei messaggi delle receive bloccate
				pcb_t *msg_list = outProcQ(&unanswered_receive, ((pcb_t *)dest));				
				insertMessage(&msg_list->msg_inbox, (msg_t*) msg);

				} else {//msg non è ne nella ready_queue che nella coda dei pcb da risvegliare ne nella pcbFree list
					((state_t *)BIOSDATAPAGE)->reg_a0 = MSGNOGOOD;
				}
			}
			//faccio il load state e ritorno al 
			LDST(((state_t *)BIOSDATAPAGE));
			((state_t *)BIOSDATAPAGE)->reg_sp += 4;

			break;
		}
		case RECEIVEMESSAGE: {
		unsigned int sender = ((state_t *)BIOSDATAPAGE)->reg_a1;
			break;
		}
		default:
			PANIC();
			break;
		}
	}
}

static void trap_handler()
{
}

static void tlb_handler()
{
}
