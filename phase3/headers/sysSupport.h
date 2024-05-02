#ifndef SYSSUPPORT_H_INCLUDED
#define SYSSUPPORT_H_INCLUDED

#include "../../headers/const.h"
#include "../../headers/listx.h"
#include "../../headers/types.h"

#include "../../phase1/headers/pcb.h"
#include "../../phase1/headers/msg.h"

#include <uriscv/liburiscv.h>

extern pcb_PTR ssi_pcb;
extern void trap_handler(state_t *s);

void send_message_to_ssi(unsigned int payload);
void general_exception_handler();

#endif
