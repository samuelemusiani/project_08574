#ifndef UTILS3_H_INCLUDED
#define UTILS3_H_INCLUDED

#include "../../headers/const.h"
#include "../../headers/listx.h"
#include "../../headers/types.h"

#include "../../phase1/headers/pcb.h"
#include "../../phase1/headers/msg.h"

#include <uriscv/liburiscv.h>
#include <uriscv/arch.h>

#define SELF NULL
#define GET_ASID ((getENTRYHI() >> ASIDSHIFT) & 0x4f)

void p_term(pcb_t *arg);

#endif
