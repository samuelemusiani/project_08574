#include "headers/vmSupport.h"
#include "headers/utils3.h"
#include "headers/initProc.h"
#include "headers/sysSupport.h"
#include <uriscv/liburiscv.h>

swap_t swap_pool_table[POOLSIZE];
unsigned int text_pages[UPROCMAX] = { -1 };

memaddr swap_pool;

static size_tt getFrameIndex();
static void read_write_flash(memaddr ram_address, unsigned int disk_block,
			     unsigned int asid, int is_write);
static int isFrameFree(swap_t *frame);
static void update_tlb(pteEntry_t *pte);
static unsigned int number_of_pages_for_text(unsigned int a);
static int need_dirty(unsigned int asid, unsigned int missing_page);
static int is_dirty(unsigned int entry_lo);
static int find_page_by_entryhi(unsigned int entry_hi);
static int seek_entryhi_index_on_pagetable(unsigned int entry_hi, support_t *s);

// Processo mutex
void mutex_proc()
{
	mutex_payload_t p;
	while (1) {
		pcb_t *sender = (pcb_t *)SYSCALL(RECEIVEMESSAGE, ANYMESSAGE,
						 (unsigned int)&p.payload, 0);
		if (p.fields.p != 1)
			continue;

		SYSCALL(SENDMESSAGE, (unsigned int)sender, 0, 0);

		do {
			SYSCALL(RECEIVEMESSAGE, (unsigned int)sender,
				(unsigned int)&p.payload, 0);
		} while (p.fields.v != 1);
		SYSCALL(SENDMESSAGE, (unsigned int)sender, 0, 0);
	}
}

void tlb_handler()
{
	ssi_payload_t getsupportdata = { .service_code = GETSUPPORTPTR,
					 .arg = NULL };
	support_t *s;

	// Get the support structure of the current process from the SSI
	SYSCALL(SENDMESSAGE, (unsigned int)ssi_pcb,
		(unsigned int)(&getsupportdata), 0);
	SYSCALL(RECEIVEMESSAGE, (unsigned int)ssi_pcb, (unsigned int)(&s), 0);

	int asid = s->sup_asid;

	// If the cause is a TLB-Mod exception, treat it as a program trap
	if ((s->sup_exceptState[PGFAULTEXCEPT].cause & GETEXECCODE) == 24) {
		trap_handler(&s->sup_exceptState[PGFAULTEXCEPT]);
	}

	// Gain mutual exclusion by sending a message to the mutex process
	mutex_payload_t p = { .fields.p = 1 };
	SYSCALL(SENDMESSAGE, (unsigned int)mutex_pcb, p.payload, 0);
	SYSCALL(RECEIVEMESSAGE, (unsigned int)mutex_pcb, 0, 0);

	memaddr missing_page = find_page_by_entryhi(
		s->sup_exceptState[PGFAULTEXCEPT].entry_hi);

	// Choose a frame i in the swap pool
	unsigned int frame_i = getFrameIndex();

	memaddr ram_addr = swap_pool + frame_i * PAGESIZE;

	if (!isFrameFree(&swap_pool_table[frame_i])) {
		// Disable interrupts
		unsigned int status = getSTATUS();
		setSTATUS(status & ~(1 << MSTATUS_MIE_BIT));

		unsigned int elo = swap_pool_table[frame_i].sw_pte->pte_entryLO;

		// Mark the page as invalid
		swap_pool_table[frame_i].sw_pte->pte_entryLO = 0;

		update_tlb(swap_pool_table[frame_i].sw_pte);

		// Enable interrupts
		setSTATUS(status);

		// Write the contents of the frame i to the correct location on
		// process x’s backing store/flash device only if is dirty
		if (is_dirty(elo)) {
			read_write_flash(ram_addr,
					 swap_pool_table[frame_i].sw_pageNo,
					 swap_pool_table[frame_i].sw_asid, 1);
		}
	}

	// Read the contents of the Current Process’s backing
	// store/flash device logical page p into frame i
	read_write_flash(ram_addr, missing_page, asid, 0);
	if (missing_page == 0 && text_pages[asid - 1] == -1) {
		// We load for the first time the porcess
		text_pages[asid - 1] = number_of_pages_for_text(ram_addr);
	}

	unsigned int page_index = seek_entryhi_index_on_pagetable(
		s->sup_exceptState[PGFAULTEXCEPT].entry_hi, s);
	if (page_index == -1) {
		trap_handler(); // No page left, trap needed
	}

	swap_pool_table[frame_i].sw_asid = asid;
	swap_pool_table[frame_i].sw_pageNo = missing_page;
	swap_pool_table[frame_i].sw_pte = &s->sup_privatePgTbl[page_index];

	// Disable interrupts
	unsigned int status = getSTATUS();
	setSTATUS(status & ~(1 << MSTATUS_MIE_BIT));

	// Update the page table of the process
	s->sup_privatePgTbl[page_index].pte_entryHI =
		s->sup_exceptState[PGFAULTEXCEPT].entry_hi;
	s->sup_privatePgTbl[page_index].pte_entryLO =
		(ram_addr & ~(0xFFF)) | VALIDON |
		(need_dirty(asid, missing_page) ? DIRTYON : 0);

	update_tlb(&s->sup_privatePgTbl[page_index]);

	// Enable interrupts
	setSTATUS(status);

	p.fields.p = 0;
	p.fields.v = 1;
	SYSCALL(SENDMESSAGE, (unsigned int)mutex_pcb, p.payload, 0);
	SYSCALL(RECEIVEMESSAGE, (unsigned int)mutex_pcb, 0, 0);

	LDST(&s->sup_exceptState[PGFAULTEXCEPT]);
}

static int isFrameFree(swap_t *frame)
{
	return frame->sw_asid == NOPROC;
}

static void update_tlb(pteEntry_t *pte)
{
	setENTRYHI(pte->pte_entryHI);
	TLBP();
	// The PRESENTFLAG is wrong, it checks if it is not present. So we use
	// a not
	if (!(getINDEX() & PRESENTFLAG)) {
		setENTRYLO(pte->pte_entryLO);
		TLBWI();
	}
}

static size_tt getFrameIndex()
{
	// Cerco un frame libero
	for (size_tt i = 0; i < POOLSIZE; i++) {
		if (isFrameFree(&swap_pool_table[i])) {
			return i;
		}
	}

	// Se non ci sono frame liberi
	static size_tt frame_index = -1;
	frame_index = (frame_index + 1) % POOLSIZE;
	return frame_index;
}

static void read_write_flash(memaddr ram_address, unsigned int disk_block,
			     unsigned int asid, int is_write)
{
	devreg_t *base = (devreg_t *)DEV_REG_ADDR(IL_FLASH, asid - 1);
	unsigned int *data0 = &(base->dtp.data0);
	unsigned int *command_addr = &(base->dtp.command);
	unsigned int status;

	ssi_do_io_t data0_doio = {
		.commandAddr = data0,
		.commandValue = ram_address,
	};
	ssi_payload_t data0_payload = {
		.service_code = DOIO,
		.arg = &data0_doio,
	};
	SYSCALL(SENDMESSAGE, (unsigned int)ssi_pcb,
		(unsigned int)(&data0_payload), 0);

	ssi_do_io_t command_doio = {
		.commandAddr = command_addr,
		.commandValue = disk_block << 8 |
				(is_write ? WRITEBLK : READBLK),
	};
	ssi_payload_t command_payload = {
		.service_code = DOIO,
		.arg = &command_doio,
	};

	SYSCALL(SENDMESSAGE, (unsigned int)ssi_pcb,
		(unsigned int)(&command_payload), 0);
	SYSCALL(RECEIVEMESSAGE, (unsigned int)ssi_pcb, (unsigned int)(&status),
		0);

	if (status != 1)
		trap_handler();
}

void mark_free_pages(unsigned int asid)
{
	unsigned int status = getSTATUS();
	setSTATUS(status & ~(1 << MSTATUS_MIE_BIT));

	for (int i = 0; i < POOLSIZE; i++) {
		if (swap_pool_table[i].sw_asid == asid)
			swap_pool_table[i].sw_asid = NOPROC;
	}

	setSTATUS(status);
	return;
}

void initSwapStructs()
{
	// Init swap pool table
	// Use the find_start_addr.sh script to calculate the address below
	swap_pool = (memaddr)0x2000C000;

	for (int i = 0; i < POOLSIZE; i++) {
		swap_pool_table[i].sw_asid = NOPROC;
	}
}

// This function reads the number of pages required for
// the .text area of an u-proc
static unsigned int number_of_pages_for_text(unsigned int a)
{
	return *((unsigned int *)(a + 0x014)) / PAGESIZE;
}

// We save the number of pages used by the text area in order to check
// if a page needs to be dirty
static int need_dirty(unsigned int asid, unsigned int missing_page)
{
	return text_pages[asid - 1] <= missing_page;
}

static int is_dirty(unsigned int entry_lo)
{
	return entry_lo & DIRTYON;
}

static int find_page_by_entryhi(unsigned int entry_hi)
{
	unsigned int tmp = entry_hi >> VPNSHIFT;
	unsigned int missing_page;
	if (tmp > 0x80000 + PAGESIZE * MAXPAGES) { // Stack page
		missing_page = 31 + (0xBFFFF - tmp);
	} else { // Program page
		missing_page = (tmp - 0x80000);
	}

	return missing_page;
}

// This function is needed as we save pages not in the index of the array
// to allow more stack pages
static int seek_entryhi_index_on_pagetable(unsigned int entry_hi, support_t *s)
{
	for (int i = 0; i < MAXPAGES; i++) {
		if (s->sup_privatePgTbl[i].pte_entryHI == 0 ||
		    s->sup_privatePgTbl[i].pte_entryHI == entry_hi) {
			return i;
		}
	}
	return -1; // no page left
}
