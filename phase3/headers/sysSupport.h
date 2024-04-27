#ifndef SYSSUPPORT_H_INCLUDED
#define SYSSUPPORT_H_INCLUDED

#include "../../headers/const.h"
#include "../../headers/listx.h"
#include "../../headers/types.h"

#include "../../phase1/headers/pcb.h"
#include "../../phase1/headers/msg.h"
#include "../../phase2/headers/ssi.h"
#include "../../phase2/headers/initial.h"
#include "../../phase2/headers/scheduler.h"

#include <uriscv/cpu.h>
#include <uriscv/arch.h>
#include <uriscv/liburiscv.h>

void send_message_to_ssi(unsigned int payload);
void general_exception_handler();

#endif
