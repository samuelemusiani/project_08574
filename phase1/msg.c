#include "./headers/msg.h"

static msg_t msgTable[MAXMESSAGES];
LIST_HEAD(msgFree_h);

// insert the element pointed to by m onto the msgFree list.
void freeMsg(msg_t *m)
{
	list_add(&m->m_list, &msgFree_h);
}

// return NULL if the msgFree list is empty. Otherwise, remove an element from
// the msgFree list, provide initial values for ALL of the messages fields and
// then return a pointer to the removed element. Messages get reused, so it is
// important that no previous value persist in a message when it gets
// reallocated.
msg_t *allocMsg()
{
	if (list_empty(&msgFree_h))
		return NULL;

	msg_t *tmp = container_of(msgFree_h.next, msg_t, m_list);
	list_del(&tmp->m_list);

	INIT_LIST_HEAD(&tmp->m_list);
	tmp->m_sender = NULL;
	tmp->m_payload = 0;

	return tmp;
}

// initialize the msgFree list to contain all the elements of the static array
// of MAXMESSAGES messages. This method will be called only once during data
// structure initialization.
void initMsgs()
{
	for (int i = 0; i < MAXMESSAGES; i++)
		list_add(&msgTable[i].m_list, &msgFree_h);
}

// used to initialize a variable to be head pointer to a message queue; returns
// a pointer to the head of an empty message queue.
void mkEmptyMessageQ(struct list_head *head)
{
	return INIT_LIST_HEAD(head);
}

// returns TRUE if the queue whose tail is pointed to by head is empty, FALSE
// otherwise.
int emptyMessageQ(struct list_head *head)
{
	return list_empty(head);
}

// insert the message pointed to by m at the end (tail) of the queue whose head
// pointer is pointed to by head.
void insertMessage(struct list_head *head, msg_t *m)
{
	list_add_tail(&m->m_list, head);
}

//  insert the message pointed to by m at the head of the queue whose head
//  pointer is pointed to by head
void pushMessage(struct list_head *head, msg_t *m)
{
	list_add(&m->m_list, head);
}

// remove the first element (starting by the head) from the message queue
// accessed via head whose sender is p_ptr. If p_ptr is NULL, return the first
// message in the queue. Return NULL if the message queue was empty or if no
// message from p ptr was found; otherwise return the pointer to the removed
// message.
msg_t *popMessage(struct list_head *head, pcb_t *p_ptr)
{
	if (list_empty(head))
		return NULL;

	if (p_ptr == NULL) {
		msg_t *tmp = container_of(list_next(head), msg_t, m_list);
		list_del(&tmp->m_list);
		return tmp;
	}

	struct list_head *iter;
	list_for_each(iter, head)
	{
		msg_t *tmp = container_of(iter, msg_t, m_list);
		if (tmp->m_sender == p_ptr) {
			list_del(&tmp->m_list);
			return tmp;
		}
	}

	return NULL;
}

// return a pointer to the first message from the queue whose head is pointed
// to by head. Do not remove the message from the queue. Return NULL if the
// queue is empty.
msg_t *headMessage(struct list_head *head)
{
	if (list_empty(head))
		return NULL;

	return container_of(list_next(head), msg_t, m_list);
}

// search the first element from the message queue accessed via head whose sender is p_ptr.
// Return 0 if p_ptr is NULL or the message queue was empty or if no message from p ptr was found;
// otherwise return 1.
int searchMsg(struct list_head *head, pcb_t *p_ptr)
{
	if (list_empty(head))
		return 0;

	if (p_ptr == NULL)
		return 0;

	struct list_head *iter;
	list_for_each(iter, head)
	{
		msg_t *tmp = container_of(iter, msg_t, m_list);
		if (tmp->m_sender == p_ptr) {
			return 1;
		}
	}

	return 0;
}
