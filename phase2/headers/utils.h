#ifndef UTILS_H_INCLUDED
#define UTILS_H_INCLUDED

#include "../../headers/const.h"
#include "../../headers/listx.h"
#include "../../headers/types.h"

#include "../../phase1/headers/pcb.h"
#include "../../phase1/headers/msg.h"

#include <uriscv/liburiscv.h>

void terminate_process(pcb_t *p);
int proc_was_in_kernel_mode(pcb_t *p);
int proc_was_in_user_mode(pcb_t *p);

#endif
