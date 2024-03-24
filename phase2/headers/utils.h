#ifndef UTILS_H_INCLUDED
#define UTILS_H_INCLUDED

#include "../../headers/const.h"
#include "../../headers/listx.h"
#include "../../headers/types.h"

#include "../../phase1/headers/pcb.h"
#include "../../phase1/headers/msg.h"

#include <uriscv/liburiscv.h>

void terminate_process(pcb_t *p);
int comm_add_to_number(memaddr command_addr);
int hash_from_device_type_number(int type, int number, int transm);
void update_cpu_time();

#endif
