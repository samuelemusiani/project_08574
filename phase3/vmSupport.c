#include "headers/vmSupport.h"
#include "headers/utils3.h"
#include "headers/initProc.h"
#include "headers/sysSupport.h"

extern pcb_t *ssi_pcb;

// La swap pool table è un array di swap_t entries
swap_t swap_pool_table[POOLSIZE];

memaddr swap_pool;

typedef union mutex_payload_t {
	struct {
		char p;
		char v;
	} fields;
	unsigned int payload;
} mutex_payload_t;

static size_tt getFrameIndex();
static void read_write_flash(memaddr ram_address, unsigned int disk_block,
			     unsigned int asid, int is_write);
static int isFrameFree(swap_t *frame);

void initSwapStructs()
{
	// Init swap pool table
	swap_pool = (memaddr)FLASHPOOLSTART; // TODO: Find more precise value

	for (int i = 0; i < POOLSIZE; i++) {
		swap_pool_table[i].sw_asid = NOPROC;
	}
}

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
	// Oherwise, it is a TLB-Invalid exception

	// Gain mutual exclusion by sending a message to the mutex process
	mutex_payload_t p = { .fields.p = 1 };
	SYSCALL(SENDMESSAGE, (unsigned int)mutex_pcb, p.payload, 0);
	SYSCALL(RECEIVEMESSAGE, (unsigned int)mutex_pcb, 0, 0);

	memaddr tmp = (s->sup_exceptState[PGFAULTEXCEPT].entry_hi >> VPNSHIFT);
	memaddr missing_page = (tmp - 0x80000) / PAGESIZE;
	if (tmp == 0xBFFFF) { // (0xC0000000 - PAGESIZE)>> 12
		missing_page = 31;
	}

	// Choose a frame i in the swap pool
	unsigned int frame_i = getFrameIndex();
	unsigned int status_IT;

	memaddr ram_addr = swap_pool + frame_i * PAGESIZE;

	if (!isFrameFree(&swap_pool_table[frame_i])) { // TODO: Do we need to
						       // check if it is dirty?
		// Disable interrupts
		status_IT = getSTATUS();
		setSTATUS(status_IT & ~(1 << 3));

		// Mark the page as invalid
		swap_pool_table[frame_i].sw_pte->pte_entryLO &= ~VALIDON;

		// TODO: Update the TLB
		// we need to check if the page is in the TLB and to update it
		// instead of clearing of TLB I made the function update_tlb but
		// for now we just clear the TLB
		TLBCLR();

		// Enable interrupts
		setSTATUS(status_IT);

		memaddr virtual_addr =
			swap_pool_table[frame_i].sw_pte->pte_entryHI >>
			VPNSHIFT;

		int disk_block = (virtual_addr - 0x800000B0) / PAGESIZE;
		// TODO: Check for stack pointer?

		// Write the contents of frame i to the correct location on
		// process x’s backing store/flash device
		read_write_flash(ram_addr, disk_block, asid, 1);
	}

	// Read the contents of the Current Process’s backing
	// store/flash device logical page p into frame i
	read_write_flash(ram_addr, missing_page, asid, 0);

	swap_pool_table[frame_i].sw_asid = asid;
	swap_pool_table[frame_i].sw_pageNo = missing_page;
	swap_pool_table[frame_i].sw_pte = &s->sup_privatePgTbl[missing_page];

	// Disable interrupts
	status_IT = getSTATUS();
	setSTATUS(status_IT & ~(1 << 3));

	// Update the page table of the process
	s->sup_privatePgTbl[missing_page].pte_entryHI =
		s->sup_exceptState[PGFAULTEXCEPT].entry_hi;
	s->sup_privatePgTbl[missing_page].pte_entryLO = (ram_addr & ~(0xFFF)) |
							VALIDON | DIRTYON;

	// Update the TLB, now we just clear the TLB
	// update_tlb(&s->sup_privatePgTbl[missing_page])??
	TLBCLR();

	// Enable interrupts
	setSTATUS(status_IT);

	p.fields.p = 0;
	p.fields.v = 1;
	SYSCALL(SENDMESSAGE, (unsigned int)mutex_pcb, p.payload, 0);
	SYSCALL(RECEIVEMESSAGE, (unsigned int)mutex_pcb, 0, 0);

	LDST(&s->sup_exceptState[PGFAULTEXCEPT]);
}

static int isFrameFree(swap_t *frame)
{
	return frame->sw_asid == -1;
}

/*
// We should use this function to update the TLB as explained in the specs
// PLEASE CHECK ME BEFORE USAGE
void update_tlb(pteEntry_t *pte)
{
	// imposto in EntryHI CP0 il valore da cercare nel TLB
	setENTRYHI(pte->pte_entryHI);
	// TLB probe command that searches for the entry in the TLB
	TLBP();

	// Se l'entry è presente nel TLB (Index.P == 0), la aggiorno
	if (getINDEX() == 0) {
		// To modify the content of the TLB, we need to write value into
		// the CP0 registers
		setENTRYHI(pte->pte_entryHI);
		setENTRYLO(pte->pte_entryLO);
		// TLB write index command that writes the entry in the correct
		// TLB index
		TLBWI();
	} else {
		PANIC();
	}
}
*/

// PandOS replacement algorithm round robin
static size_tt getFrameIndex()
{
	// Cerco un frame libero
	for (size_tt i = 0; i < POOLSIZE - 1; i++) {
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
	// Load in ram the source code of the child process
	// (flash device number [ASID])
	devreg_t *base = (devreg_t *)DEV_REG_ADDR(IL_FLASH, asid - 1);
	unsigned int *data0 = &(base->dtp.data0);
	unsigned int *command_addr = &(base->dtp.command);
	unsigned int status;

	// these lines will load the entire flash memory in RAM
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
		trap_handler(NULL);
}
