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

typedef union mutex_payload_t {
	struct {
		char p;
		char v;
	} fields;
	unsigned int payload;
} mutex_payload_t;

void initSwapStructs();
void mutex_proc();
void tlb_handler();
void mark_free_pages(unsigned int asid);

#endif
