#include "headers/sysSupport.h"
#include <uriscv/cpu.h>
#include "../phase2/headers/initial.h"

static void syscall_handler();
static void trap_handler();

void general_exception_handler()
{
	// update_cpu_time();

	// get the context from the support level struct
	state_t *s = &current_process->p_supportStruct
			      ->sup_exceptState[GENERALEXCEPT];

	unsigned int excCode = s->cause;

	if ((excCode <= 7) || (excCode > 11 && excCode < 24)) {
		trap_handler();
	} else if (excCode == 8) {
		syscall_handler();
	} else {
		// If we could not handle the exception, we panic
		PANIC();
	}
}

static void trap_handler()
{
	pass_up_or_die(GENERALEXCEPT);
}

// TLB Exception Handler
static void tlb_handler()
{
	pass_up_or_die(PGFAULTEXCEPT);
}

static void syscall_handler()
{
	switch (/* supportCauseReg */) {
	case SENDMESSAGE: {
		break;
	}
	case RECEIVEMESSAGE: {
		break;
	}
	default:
		PANIC();
		break;
	}
}
