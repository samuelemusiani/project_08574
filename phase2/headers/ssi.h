#ifndef SSI_H_INCLUDED
#define SSI_H_INCLUDED

#include "../../headers/const.h"
#include "../../headers/listx.h"
#include "../../headers/types.h"

#include "../../phase1/headers/pcb.h"
#include "../../phase1/headers/msg.h"

#include <uriscv/liburiscv.h>

#define INTERRUPT_HANDLER_MSG 0x1 // Fake address for interrupt handler
typedef union interrupt_handler_io_msg {
	struct {
		char service; // 0 doio, 1 clock
		char status; // Status of device
		char device_type; // Device type {0..5}
		char il; // Interrupt line {0..7}
	} fields;
	unsigned int payload;
} interrupt_handler_io_msg_t;

void ssi();
int should_pcb_be_soft_blocked(pcb_t *p);
int was_pcb_soft_blocked(pcb_t *p);

#endif
