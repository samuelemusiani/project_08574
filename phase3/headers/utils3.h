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
#define QPAGE 1024

#define ASIDMASK 0x40

#define WRITESTATUSMASK 0xFF

#define READBLK 2
#define WRITEBLK 3

void p_term(pcb_t *arg);
pcb_PTR p_create(state_t *state, support_t *support);
#endif
