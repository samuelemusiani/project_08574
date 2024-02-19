#include "headers/exceptions.h"
#include "headers/initial.h"
#include "headers/interrupts.h"
#include "headers/utils.h"
#include "headers/scheduler.h"
#include <uriscv/types.h>
#include <uriscv/cpu.h>

static void trap_handler();
static void syscall_handler();
static void tlb_handler();

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
	} else {
		state_t s = current_process->p_s; //salvo lo stato del processo
		unsigned int sys_type =
			s.reg_a0; //prendo dal registro a0 il tipo di syscall

		if (sys_type) { //send
			SYSCALL(SENDMESSAGE, (unsigned int)s.reg_a1,
				(unsigned int)s.reg_a2, 0);
		} else { //receive bloccate
			SYSCALL(RECEIVEMESSAGE, (unsigned int)s.reg_a1,
				(unsigned int)s.reg_a2, 0);
			//bloccare la syscall
			//copiare lo stato del processo (che sta nella BIOS data page) nello stato del processo corrente
			LDST((state_t *)BIOSDATAPAGE);
			current_process++;
			//aggiorno il CPU timer per il current process 
			//current_process->p_time = 1; TODO
			//chiamo lo scheduler
			scheduler();
		}
	}
}

static void trap_handler()
{
}

static void tlb_handler()
{
}
