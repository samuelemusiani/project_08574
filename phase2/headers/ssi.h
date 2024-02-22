#ifndef SSI_H_INCLUDED
#define SSI_H_INCLUDED

#include "../../headers/const.h"
#include "../../headers/listx.h"
#include "../../headers/types.h"

#include "../../phase1/headers/pcb.h"
#include "../../phase1/headers/msg.h"

#include <uriscv/liburiscv.h>

#define INTERRUPT_HANDLER_MSG 0x1 // Fake address for interrupt handler
typedef struct interrupt_handler_io_msg {
	int device_number;
	int status;
} interrupt_handler_io_msg_t;

void ssi();
int should_pcb_be_soft_blocked(pcb_t *p);

#endif
