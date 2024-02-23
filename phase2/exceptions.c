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
static void blockSys(unsigned int sender);
static int deviceListEmpty();

LIST_HEAD(unanswered_receive);

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
		// Generate fake interrupt
		setCAUSE(((state_t *)BIOSDATAPAGE)->cause);
		trap_handler();
	} else {
		unsigned int dest = ((state_t *)BIOSDATAPAGE)->reg_a1;
		unsigned int msg = ((state_t *)BIOSDATAPAGE)->reg_a2;
		switch (((state_t *)BIOSDATAPAGE)->reg_a0) {
		case SENDMESSAGE: {
			if (searchPcb(&pcbFree_h, ((pcb_t *)dest))) {
				((state_t *)BIOSDATAPAGE)->reg_a0 =
					DEST_NOT_EXIST;
			} else {
				if (searchPcb(&ready_queue,((pcb_t *)dest))) { // cerco nella ready queue il processo che mi interessa
					//devo modificare il registro inbox del pcb a1 (destination) con il payload a2
					insertMessage(&((pcb_t *)dest)->msg_inbox,(msg_t *)msg);

				} else if (searchPcb(&unanswered_receive,((pcb_t *)dest))) { //cerco nella coda dei messaggi delle receive bloccate
					pcb_t *msg_list = outProcQ(&unanswered_receive, ((pcb_t *)dest));
					insertMessage(&msg_list->msg_inbox,(msg_t *)msg);

				} else if (searchPcb(&pcb_blocked_on_clock,((pcb_t *)dest))) { //cerco un elemento nella coda dei pcb fermati per il tempo
					pcb_t *msg_list = outProcQ(&pcb_blocked_on_clock,((pcb_t *)dest));
					insertMessage(&msg_list->msg_inbox,(msg_t *)msg);
				} else if (deviceListEmpty()) { //cerco un elemento nella coda dei pcb device, aumento il softblockcount se c'è un campo dentro al messaggio dopo che ho castato a struttra payload (da vedere) in cui c'è un codice che mi dice quale probelma c'è stato (es:DOIO)
					insertMessage(&((pcb_t *)dest)->msg_inbox,(msg_t *)msg);

				} else { //idk when it should occur but i'll figure it out someday
					((state_t *)BIOSDATAPAGE)->reg_a0 =	MSGNOGOOD;
				}
			}
			//faccio il load state e ritorno al Nucleo spostando lo stack pointer di +4
			LDST(((state_t *)BIOSDATAPAGE));
			((state_t *)BIOSDATAPAGE)->reg_sp += 4;

			break;
		}
		case RECEIVEMESSAGE: {
			unsigned int sender = ((state_t *)BIOSDATAPAGE)->reg_a1;
			if (emptyProcQ(
				    &((pcb_t *)sender)->msg_inbox)) { //la lista dei messaggi è vuota blocco il processo mettendolo nella mia coda
				blockSys(sender);
			} else if (sender) { //caso sender != 0 -> non ANYMESSAGE), devo cercare nella mia inbox se c'è il messaggio del sender nella inbox
				if (searchMsg(&((pcb_t *)sender)->msg_inbox,
					    ((pcb_t *)sender))) { //se c'è il messaggio 1 else
					((state_t *)BIOSDATAPAGE)->reg_a2 =	((unsigned int)popMessage(&((pcb_t *)sender)->msg_inbox,((pcb_t *)sender)));
				} else { //non c'è il messaggio che del sender che sto cercando nella inbox
					blockSys(sender);
				}
			} else { //la lista dei messaggi non è vuota, prendo il primo messaggio della inbox
				//metto il messaggio nel registro a2 della BIOSDATA page, il msg e casto ad intero per metterlo nella BIOSDATA page
				((state_t *)BIOSDATAPAGE)->reg_a2 =	((unsigned int)headMessage(&((pcb_t *)sender)->msg_inbox));
			}
			LDST(((state_t *)BIOSDATAPAGE));
			((state_t *)BIOSDATAPAGE)->reg_sp += 4;
			break;
		}
		default:
			PANIC();
			break;
		}
	}
}

//cerca se nella lista di liste di device c'è almeno un elemento. Se c'è ritorno 1 else 0.
static int deviceListEmpty()
{
	for (int i = 0; i < MAXDEVICE; i++) {
		if (!emptyProcQ(&pcb_blocked_on_device[i])) { //problema della memcpy
			return 1;
		}
	}
	return 0;
}

//blocca una syscall (capitolo 5.5)
static void blockSys(unsigned int sender)
{
	insertProcQ(&unanswered_receive, ((pcb_t *)sender));
	insertProcQ(&ready_queue, ((pcb_t *)sender));
	current_process->p_s = *((state_t *)BIOSDATAPAGE);
	//if (funzione di sammy()) softblock_count++;
	//Aggiorno il CPU time
	scheduler();
}

static void trap_handler()
{
}

static void tlb_handler()
{
}
