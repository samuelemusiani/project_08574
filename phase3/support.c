#include "headers/support.h"

support_t support_table[UPROCMAX];

LIST_HEAD(supportFree_h);

void freeSupport(support_t *s)
{
	unsigned int status = getSTATUS();
	setSTATUS(status & ~(1 << 3));
	list_add(&s->s_list, &supportFree_h);
	setSTATUS(status);
}

support_t *allocSupport()
{
	unsigned int status = getSTATUS();
	setSTATUS(status & ~(1 << 3));
	if (list_empty(&supportFree_h)) {
		setSTATUS(status);
		return NULL;
	}
	support_t *s = container_of(supportFree_h.next, support_t, s_list);
	list_del(&s->s_list);

	for (int i = 0; i < MAXPAGES; i++) {
		s->sup_privatePgTbl[i].pte_entryHI = 0;
	}

	setSTATUS(status);
	return s;
}

void initSupports()
{
	unsigned int status = getSTATUS();
	setSTATUS(status & ~(1 << 3));
	for (int i = 0; i < UPROCMAX; i++)
		list_add(&support_table[i].s_list, &supportFree_h);
	setSTATUS(status);
}
