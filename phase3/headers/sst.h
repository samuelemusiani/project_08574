#ifndef SST_H_INCLUDED
#define SST_H_INCLUDED

#include "../../headers/const.h"
#include "../../headers/listx.h"
#include "../../headers/types.h"

#include "../../phase1/headers/pcb.h"
#include "../../phase1/headers/msg.h"

#include <uriscv/liburiscv.h>
#include <uriscv/arch.h>

void sst();
extern pcb_PTR sst_pcbs[UPROCMAX];
#endif
