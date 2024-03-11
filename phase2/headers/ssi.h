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
		char service;	    // 0 doio, 1 clock
		char status;	    // Status of device
		char device_type;   // Device type {0..4}
		char device_number; // Interrupt line {0..6}
	} fields;
	unsigned int payload;
} interrupt_handler_io_msg_t;

#define SSI_FAKE_ADDR (pcb_t *)0x2

void ssi();
int is_a_softblocking_request(ssi_payload_t *p);

#endif
