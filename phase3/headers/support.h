#ifndef SUPPORT_H_INCLUDED
#define SUPPORT_H_INCLUDED

#include "../../headers/const.h"
#include "../../headers/listx.h"
#include "../../headers/types.h"

#include "../../phase1/headers/pcb.h"
#include "../../phase1/headers/msg.h"

#include <uriscv/liburiscv.h>
#include <uriscv/arch.h>

void freeSupport(support_t *s);
support_t *allocSupport();
void initSupports();

#endif
