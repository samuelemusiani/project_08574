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

// initialize the pcbFree list to contain all the elements of the static array
// of MAXPROC PCBs. This method will be called only once during data structure
// initialization.
void initPcbs() {
  for (int i = 0; i < MAXPROC; i++)
    list_add(&pcbTable[i].p_list, &pcbFree_h);
}

// this method is used to initialize a variable to be head pointer to a process
// queue.
void mkEmptyProcQ(struct list_head *head) {
  // This is too easy...
  INIT_LIST_HEAD(head);
}

// Return TRUE if the queue whose head is pointed to by head is empty. Return
// FALSE otherwise.
int emptyProcQ(struct list_head *head) { return list_empty(head); }

// Insert the PCB pointed by p into the process queue whose head pointer is
// pointed to by head.
void insertProcQ(struct list_head *head, pcb_t *p) {
  list_add(&p->p_list, head);
}

// Return a pointer to the first PCB from the process queue whose head is
// pointed to by head. Do not remove this PCB from the process queue. Return
// NULL if the process queue is empty.
pcb_t *headProcQ(struct list_head *head) {
  if (list_empty(head))
    return NULL;

  return container_of(&head->next, pcb_t, p_list);
}

// Remove the first (i.e. head) element from the process queue whose head
// pointer is pointed to by head. Return NULL if the process queue was
// initially empty; otherwise return the pointer to the removed element.
pcb_t *removeProcQ(struct list_head *head) {
  if (list_empty(head))
    return NULL;

  pcb_t *tmp = headProcQ(head);
  list_del(&tmp->p_list);
  return tmp;
}

// Remove the PCB pointed to by p from the process queue whose head pointer is
// pointed to by head. If the desired entry is not in the indicated queue (an
// error condition), return NULL; otherwise, return p. Note that p can point to
// any element of the process queue.
pcb_t *outProcQ(struct list_head *head, pcb_t *p) {
  struct list_head *iter;
  list_for_each(iter, head) {
    if (container_of(iter, pcb_t, p_list) == p)
      return removeProcQ(iter);
  }

  return NULL;
}

int emptyChild(pcb_t *p) {}

void insertChild(pcb_t *prnt, pcb_t *p) {}

pcb_t *removeChild(pcb_t *p) {}

pcb_t *outChild(pcb_t *p) {}
