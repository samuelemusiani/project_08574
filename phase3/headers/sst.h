#ifndef SST_H_INCLUDED
#define SST_H_INCLUDED

#include "../../headers/const.h"
#include "../../headers/listx.h"
#include "../../headers/types.h"

#include "../../phase1/headers/pcb.h"
#include "../../phase1/headers/msg.h"

#include <uriscv/liburiscv.h>
#include <uriscv/arch.h>

// ???
typedef union sst_child_t {
	struct {
		unsigned int state;
		unsigned int support;
	} fields;
	unsigned int payload;
} sst_child_t;

void sst();
extern pcb_PTR sst_pcbs[UPROCMAX];
#endif
