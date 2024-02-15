#ifndef INITIAL_H_INCLUDED
#define INITIAL_H_INCLUDED

#include "../../headers/const.h"
#include "../../headers/listx.h"
#include "../../headers/types.h"

#include "../../phase1/headers/pcb.h"
#include "../../phase1/headers/msg.h"

#include <uriscv/liburiscv.h>

extern unsigned int process_count;
extern unsigned int softblock_count;
extern struct list_head ready_queue;
extern pcb_t *current_process;

// uriscv supports five different classes of external devices: disk, tape,
// network card, printer, and terminal. Furthermore, μRISCV can support up to
// eight instances of each device type.
// Terminal devices are actually two independent sub-devices,
#define MAXDEVICE 48 // (4 + 1 * 2) * 8

extern struct list_head pcb_blocked_on_device[MAXDEVICE]; // Array of queues
extern struct list_head pcb_blocked_on_clock;

extern void test();

#endif
