#include "./headers/pcb.h"
#include <uriscv/liburiscv.h>

/*
 * Every function that manipulates the PCBs must be executed with interrupts
 * disabled. In fact, every function begins and ends in the same manner:
 * unsigned int status = getSTATUS();
 * setSTATUS(status & ~(1 << 3));
 * ...
 * setSTATUS(status);
 */

pcb_t pcbTable[MAXPROC]; /* PCB array with maximum size 'MAXPROC' */
LIST_HEAD(pcbFree_h);	 /* List of free PCBs                     */
static int next_pid = 1;

// Insert the element pointed to by p onto the pcbFree list.
void freePcb(pcb_t *p)
{
	unsigned int status = getSTATUS();
	setSTATUS(status & ~(1 << 3));
	list_add(&p->p_list, &pcbFree_h);
	setSTATUS(status);
}

/*
 * Return NULL if the pcbFree list is empty. Otherwise, remove an element from
 * the pcbFree list, provide initial values for ALL of the PCBs fields (i.e.
 * NULL and/or 0) and then return a pointer to the removed element. PCBs get
 * reused, so it is important that no previous value persist in a PCB when it
 * gets reallocated
 */
pcb_t *allocPcb()
{
	unsigned int status = getSTATUS();
	setSTATUS(status & ~(1 << 3));
	if (list_empty(&pcbFree_h)) {
		setSTATUS(status);
		return NULL;
	}
	pcb_t *p = container_of(pcbFree_h.next, pcb_t, p_list);
	list_del(&p->p_list);

	INIT_LIST_HEAD(&p->p_list);
	p->p_parent = NULL;
	INIT_LIST_HEAD(&p->p_child);
	INIT_LIST_HEAD(&p->p_sib);

	p->p_s.entry_hi = 0;
	p->p_s.cause = 0;
	p->p_s.status = 0;
	p->p_s.pc_epc = 0;
	p->p_s.mie = 0;
	for (int i = 0; i < STATE_GPR_LEN; i++) {
		p->p_s.gpr[i] = 0;
	}

	p->p_time = 0;

	INIT_LIST_HEAD(&p->msg_inbox);
	p->p_supportStruct = NULL;

	p->p_pid = next_pid;
	next_pid++;

	p->do_io = 0;
	INIT_LIST_HEAD(&p->p_io);
	setSTATUS(status);
	return p;
}

/*
 * Initialize the pcbFree list to contain all the elements of the static array
 * of MAXPROC PCBs. This method will be called only once during data structure
 * initialization.
 */
void initPcbs()
{
	unsigned int status = getSTATUS();
	setSTATUS(status & ~(1 << 3));
	for (int i = 0; i < MAXPROC; i++)
		list_add(&pcbTable[i].p_list, &pcbFree_h);
	setSTATUS(status);
}

/*
 * This method is used to initialize a variable to be head pointer to a process
 * queue.
 */
void mkEmptyProcQ(struct list_head *head)
{
	unsigned int status = getSTATUS();
	setSTATUS(status & ~(1 << 3));
	INIT_LIST_HEAD(head);
	setSTATUS(status);
}

/*
 * Return TRUE if the queue whose head is pointed to by head is empty. Return
 * FALSE otherwise.
 */
int emptyProcQ(struct list_head *head)
{
	unsigned int status = getSTATUS();
	setSTATUS(status & ~(1 << 3));
	int a = list_empty(head);
	setSTATUS(status);
	return a;
}

/*
 * Insert the PCB pointed by p into the process queue whose head pointer is
 * pointed to by head.
 */
void insertProcQ(struct list_head *head, pcb_t *p)
{
	unsigned int status = getSTATUS();
	setSTATUS(status & ~(1 << 3));
	list_add_tail(&p->p_list, head);
	setSTATUS(status);
}

void insertProcQForIO(struct list_head *head, pcb_t *p)
{
	unsigned int status = getSTATUS();
	setSTATUS(status & ~(1 << 3));
	list_add_tail(&p->p_io, head);
	setSTATUS(status);
}

/*
 * Return a pointer to the first PCB from the process queue whose head is
 * pointed to by head. Do not remove this PCB from the process queue. Return
 * NULL if the process queue is empty.
 */
pcb_t *headProcQ(struct list_head *head)
{
	unsigned int status = getSTATUS();
	setSTATUS(status & ~(1 << 3));
	if (list_empty(head)) {
		setSTATUS(status);
		return NULL;
	}
	pcb_t *a = container_of(list_next(head), pcb_t, p_list);
	setSTATUS(status);
	return a;
}

pcb_t *headProcQForIO(struct list_head *head)
{
	unsigned int status = getSTATUS();
	setSTATUS(status & ~(1 << 3));
	if (list_empty(head)) {
		setSTATUS(status);
		return NULL;
	}
	pcb_t *a = container_of(list_next(head), pcb_t, p_io);
	setSTATUS(status);
	return a;
}

/*
 * Remove the first (i.e. head) element from the process queue whose head
 * pointer is pointed to by head. Return NULL if the process queue was
 * initially empty; otherwise return the pointer to the removed element.
 */
pcb_t *removeProcQ(struct list_head *head)
{
	unsigned int status = getSTATUS();
	setSTATUS(status & ~(1 << 3));
	if (list_empty(head)) {
		setSTATUS(status);
		return NULL;
	}

	pcb_t *tmp = headProcQ(head);
	list_del(&tmp->p_list);
	INIT_LIST_HEAD(&tmp->p_list);
	setSTATUS(status);
	return tmp;
}

pcb_t *removeProcQForIO(struct list_head *head)
{
	unsigned int status = getSTATUS();
	setSTATUS(status & ~(1 << 3));
	if (list_empty(head)) {
		setSTATUS(status);
		return NULL;
	}

	pcb_t *tmp = headProcQForIO(head);
	list_del(&tmp->p_io);
	INIT_LIST_HEAD(&tmp->p_io);
	setSTATUS(status);
	return tmp;
}

/*
 * Remove the PCB pointed to by p from the process queue whose head pointer is
 * pointed to by head. If the desired entry is not in the indicated queue (an
 * error condition), return NULL; otherwise, return p. Note that p can point to
 * any element of the process queue.
 */
pcb_t *outProcQ(struct list_head *head, pcb_t *p)
{
	unsigned int status = getSTATUS();
	setSTATUS(status & ~(1 << 3));
	struct list_head *iter;
	pcb_t *a = NULL;
	list_for_each(iter, head)
	{
		pcb_t *tmp = container_of(iter, pcb_t, p_list);
		if (tmp == p) {
			list_del(&tmp->p_list);
			a = tmp;
		}
	}
	setSTATUS(status);
	return a;
}

pcb_t *outProcQForIO(struct list_head *head, pcb_t *p)
{
	unsigned int status = getSTATUS();
	setSTATUS(status & ~(1 << 3));
	struct list_head *iter;
	pcb_t *a = NULL;
	list_for_each(iter, head)
	{
		pcb_t *tmp = container_of(iter, pcb_t, p_io);
		if (tmp == p) {
			list_del(&tmp->p_list);
			a = tmp;
		}
	}
	setSTATUS(status);
	return a;
}

/*
 * Return TRUE if the PCB pointed to by p has no children. Return FALSE
 * otherwise.
 */
int emptyChild(pcb_t *p)
{
	unsigned int status = getSTATUS();
	setSTATUS(status & ~(1 << 3));
	unsigned int a = list_empty(&p->p_child);
	setSTATUS(status);
	return a;
}

// Make the PCB pointed to by p a child of the PCB pointed to by prnt.
void insertChild(pcb_t *prnt, pcb_t *p)
{
	unsigned int status = getSTATUS();
	setSTATUS(status & ~(1 << 3));
	p->p_parent = prnt;

	list_add_tail(&p->p_sib, &prnt->p_child);
	setSTATUS(status);
}

/*
 * Make the first child of the PCB pointed to by p no longer a child of p.
 * Return NULL if initially there were no children of p. Otherwise, return a
 * pointer to this removed first child PCB.
 */
pcb_t *removeChild(pcb_t *p)
{
	unsigned int status = getSTATUS();
	setSTATUS(status & ~(1 << 3));
	pcb_t *a = removeProcQ(&p->p_child);
	setSTATUS(status);
	return a;
}

/*
 * Make the PCB pointed to by p no longer the child of its parent. If the PCB
 * pointed to by p has no parent, return NULL; otherwise, return p. Note that
 * the element pointed to by p could be in an arbitrary position (i.e. not be
 * the first child of its parent).
 */
pcb_t *outChild(pcb_t *p)
{
	unsigned int status = getSTATUS();
	setSTATUS(status & ~(1 << 3));
	if (p->p_parent == NULL) {
		setSTATUS(status);
		return NULL;
	}

	list_del(&p->p_sib);
	setSTATUS(status);
	return p;
}

// Return 1 if the pcb pointed by p is in the list whose head is pointed to by
// head. Return 0 otherwise
int searchPcb(struct list_head *head, pcb_t *p)
{
	unsigned int status = getSTATUS();
	setSTATUS(status & ~(1 << 3));
	struct list_head *iter;
	unsigned int a = 0;
	list_for_each(iter, head)
	{
		pcb_t *tmp = container_of(iter, pcb_t, p_list);
		if (tmp == p) {
			a = 1;
		}
	}
	setSTATUS(status);
	return a;
}

int isPcbValid(pcb_t *p)
{
	unsigned int status = getSTATUS();
	setSTATUS(status & ~(1 << 3));
	int a = 1;
	if (p < pcbTable || p > pcbTable + MAXPROC - 1 ||
	    searchPcb(&pcbFree_h, p) ||
	    ((int)p - (int)pcbTable) % sizeof(pcb_t) != 0)
		a = 0;
	setSTATUS(status);
	return a;
}
