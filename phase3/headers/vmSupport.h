#ifndef VMSUPPORT_H_INCLUDED
#define VMSUPPORT_H_INCLUDED

#include "../../headers/const.h"
#include "../../headers/listx.h"
#include "../../headers/types.h"

#include "../../phase1/headers/pcb.h"
#include "../../phase1/headers/msg.h"

#include <uriscv/liburiscv.h>
#include <uriscv/arch.h>

extern memaddr swap_pool;

void initSwapStructs();
void mutex_proc();
void tlb_handler();

#endif
