#include "./headers/pcb.h"

static pcb_t pcbTable[MAXPROC]; /* PCB array with maximum size 'MAXPROC' */
LIST_HEAD(pcbFree_h);           /* List of free PCBs                     */
static int next_pid = 1;

// insert the element pointed to by p onto the pcbFree list.
void freePcb(pcb_t *p) {
  p->p_list.next = pcbFree_h.next;
  p->p_list.prev = &pcbFree_h;
  pcbFree_h.next = &(p->p_list);
  p->p_list.next->prev = &(p->p_list);
}

// return NULL if the pcbFree list is empty. Otherwise, remove an element from
// the pcbFree list, provide initial values for ALL of the PCBs fields and then
// return a pointer to the removed element. PCBs get reused, so it is important
// that no previous value persist in a PCB when it gets reallocated.

#define INIT_PS_STATE(state)                                                   \
  {                                                                            \
    state->entry_ji = 0;                                                       \
    state->cause = 0;                                                          \
    state->status = 0;                                                         \
    state->pc_epc = 0;                                                         \
    state->mie = 0;                                                            \
    for (int _i = 0; _i < STATE_GPR_LEN; _i++) {                               \
      state->gpr[_i] = 0;                                                      \
    }                                                                          \
  }

pcb_t *allocPcb() {
  if (pcbFree_h.next == &pcbFree_h /* && pcbFree_h.prev == &pcbFree_h */)
    return NULL;

  pcb_t *p = container_of(pcbFree_h.next, pcb_t, p_list);
  pcbFree_h.next = p->p_list.next;
  pcbFree_h.next->prev = &pcbFree_h;

  INIT_LIST_HEAD(&p->p_list);
  p->p_parent = NULL;
  INIT_LIST_HEAD(&p->p_child);
  INIT_LIST_HEAD(&p->p_sib);

  // NON WORKA
  INIT_PS_STATE(&(p->p_s));
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
