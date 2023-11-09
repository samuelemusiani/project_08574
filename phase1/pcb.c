#include "./headers/pcb.h"

static pcb_t pcbTable[MAXPROC]; /* PCB array with maximum size 'MAXPROC' */
LIST_HEAD(pcbFree_h);           /* List of free PCBs                     */
static int next_pid = 1;

// insert the element pointed to by p onto the pcbFree list.
void freePcb(pcb_t *p) { list_add(&p->p_list, &pcbFree_h); }

// return NULL if the pcbFree list is empty. Otherwise, remove an element from
// the pcbFree list, provide initial values for ALL of the PCBs fields and then
// return a pointer to the removed element. PCBs get reused, so it is important
// that no previous value persist in a PCB when it gets reallocated.

// I don't know if this is the right way. Maybe we can do tath in a struct
#define INIT_PS_STATE(state)                                                   \
  ({                                                                           \
    (state)->entry_hi = 0;                                                     \
    (state)->cause = 0;                                                        \
    (state)->status = 0;                                                       \
    (state)->pc_epc = 0;                                                       \
    (state)->mie = 0;                                                          \
    for (int _i = 0; _i < STATE_GPR_LEN; _i++) {                               \
      (state)->gpr[_i] = 0;                                                    \
    }                                                                          \
  })

pcb_t *allocPcb() {
  if (list_empty(&pcbFree_h))
    return NULL;

  // For my lsp the following line is a working.
  // Cast to smaller integer type 'size_tt' (aka 'unsigned int') from 'struct
  // list_head *'
  pcb_t *p = container_of(pcbFree_h.next, pcb_t, p_list);
  list_del(&p->p_list);

  INIT_LIST_HEAD(&p->p_list);
  p->p_parent = NULL;
  INIT_LIST_HEAD(&p->p_child);
  INIT_LIST_HEAD(&p->p_sib);

  INIT_PS_STATE(&p->p_s);
  p->p_time = 0;

  INIT_LIST_HEAD(&p->msg_inbox);
  p->p_supportStruct = NULL;

  // This should be done here?
  p->p_pid = next_pid;
  next_pid++;

  return p;
}

void initPcbs() {}

void mkEmptyProcQ(struct list_head *head) {}

int emptyProcQ(struct list_head *head) {}

void insertProcQ(struct list_head *head, pcb_t *p) {}

pcb_t *headProcQ(struct list_head *head) {}

pcb_t *removeProcQ(struct list_head *head) {}

pcb_t *outProcQ(struct list_head *head, pcb_t *p) {}

int emptyChild(pcb_t *p) {}

void insertChild(pcb_t *prnt, pcb_t *p) {}

pcb_t *removeChild(pcb_t *p) {}

pcb_t *outChild(pcb_t *p) {}
