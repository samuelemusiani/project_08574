#include "headers/vmSupport.h"
#include "headers/utils3.h"
#include "headers/initProc.h"
#include "headers/sysSupport.h"

#define NOPROC -1

extern pcb_t *ssi_pcb;

// La swap pool table è un array di swap_t entries
swap_t swap_pool_table[POOLSIZE];

swap_t *swap_pool;

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
// static memaddr page_index_address(size_tt page_index);

void initSwapStructs()
{
	// Init swap pool table
	swap_pool = (swap_t *)FLASHPOOLSTART; // TODO: Find more precise value

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

	// TODO: Mutex process
	p_term(SELF);
}

void tlb_handler()
{
	// Devo mandare una SYSCALL a SSI per richieddere il getsupportdata
	// Devo ricevere il messaggio da SSI
	ssi_payload_t getsupportdata = { .service_code = GETSUPPORTPTR,
					 .arg = NULL };

	support_t *s;
	unsigned int status_IT;

	// richiedo il GETSUPPORTPTR
	SYSCALL(SENDMESSAGE, (unsigned int)ssi_pcb,
		(unsigned int)(&getsupportdata), 0);
	// ricevo il GETSUPPORTPTR
	SYSCALL(RECEIVEMESSAGE, (unsigned int)ssi_pcb, (unsigned int)(&s), 0);

	int asid = s->sup_asid;

	// Se la causa è una TLB-Modification exception, deve considerare
	// l'eccezione come program trap ovvero cause register ha excode 24

	// TLB-MOD
	if ((s->sup_exceptState[PGFAULTEXCEPT].cause & GETEXECCODE) == 24) {
		// TLB-Modification exception is treated as a program trap
		trap_handler(&s->sup_exceptState[PGFAULTEXCEPT]);
	}
	// Se la causa è una TLB-invalid

	// devo prendere la mutua esclusione sulla swap pool table mandando un
	// messaggio allo swap mutex e ricevendo il messaggio di conferma
	mutex_payload_t p = { .fields.p = 1 };
	SYSCALL(SENDMESSAGE, (unsigned int)mutex_pcb, (unsigned int)&p.payload,
		0);
	SYSCALL(RECEIVEMESSAGE, (unsigned int)mutex_pcb, 0, 0);

	memaddr missing_page =
		((s->sup_exceptState[PGFAULTEXCEPT].entry_hi >> VPNSHIFT) -
		 0x800000B0) /
		PAGESIZE;
	// TODO: check stack pointer that is block 31. THIS ONLY WORK FOR BLOCK
	// [0..30]

	// scegliere un frame i di ram dallo swap pool (FIFO)

	// se il frame i è occupato da una pagina logica k del processo x e la
	// pagina k è dirty (modificata) allora
	// 	- aggiorno la page table del processo x (in mutua esclusione)
	//  - aggiorno la TLB, se la pagina k del processo x è in TLB (in mutua
	//  esclusione)
	//  - scrivo il contenuto del frame i sulla posizione correta del flash
	//  device del processo x

	unsigned int frame_i = getFrameIndex();
	// memaddr frame_addr_i = page_index_address(frame_i);
	if (!isFrameFree(&swap_pool_table[frame_i])) {
		status_IT = getSTATUS();
		setSTATUS(status_IT & ~(1 << 3));

		// marco la page table entry k del processo x come non valida
		swap_pool_table[frame_i].sw_pte->pte_entryLO &= ~VALIDON;

		// se la entry k del processo x è in TLB, cancello tutte le
		// entry della TLB
		TLBCLR();

		// riabilito gli interrupt
		setSTATUS(status_IT);
	}

	if (swap_pool_table[frame_i].sw_pte == NULL) {
		PANIC();
	}

	// disabilito gli interrupt

	memaddr ram_addr = swap_pool_table[frame_i].sw_pte->pte_entryLO >> 12;

	memaddr virtual_addr = swap_pool_table[frame_i].sw_pte->pte_entryHI >>
			       VPNSHIFT;
	int disk_block = (virtual_addr - 0x800000B0) / PAGESIZE;

	// Write the contents of frame i to the correct location on process x’s
	// backing store/flash device
	read_write_flash(ram_addr, disk_block, asid, 1);

	// Read the contents of the Current Process’s backing store/flash device
	// logical page p into frame i
	read_write_flash(ram_addr, missing_page, asid, 0);

	swap_pool_table[frame_i].sw_asid = asid;
	swap_pool_table[frame_i].sw_pageNo = missing_page;
	swap_pool_table[frame_i].sw_pte = &s->sup_privatePgTbl[missing_page];

	status_IT = getSTATUS();
	setSTATUS(status_IT & ~(1 << 3));

	s->sup_privatePgTbl[missing_page].pte_entryLO = ram_addr << 12 |
							VALIDON | DIRTYON;

	TLBCLR();

	setSTATUS(status_IT);

	p.fields.p = 0;
	p.fields.v = 1;
	SYSCALL(SENDMESSAGE, (unsigned int)mutex_pcb, (unsigned int)&p.payload,
		0);
	SYSCALL(RECEIVEMESSAGE, (unsigned int)mutex_pcb, 0, 0);

	LDST(&s->sup_exceptState[PGFAULTEXCEPT]);
}
/*
static int isvalidAsid(int asid)
{
	return asid > 0 && asid <= UPROCMAX;
}
*/
static int isFrameFree(swap_t *frame)
{
	return frame->sw_asid == -1;
}

/* Convert page_index [0, 16] to the corresponding address in the swap pool
static memaddr page_index_address(size_tt page_index)
{
	if (page_index < 0 || page_index >= POOLSIZE) {
		return (memaddr)NULL;
	} else {
		return (0x20020000 + (page_index * PAGESIZE));
	}
}

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

	// Se non ci sono frame liberi, uso la politica FIFO
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

	// .text? .data? idk
	// these lines will load the entire flash memory in RAM
	ssi_do_io_t do_io = {
		.commandAddr = data0,
		.commandValue = ram_address,
	};
	ssi_payload_t payload = {
		.service_code = DOIO,
		.arg = &do_io,
	};
	SYSCALL(SENDMESSAGE, (unsigned int)ssi_pcb, (unsigned int)(&payload),
		0);
	SYSCALL(RECEIVEMESSAGE, (unsigned int)ssi_pcb, (unsigned int)(&status),
		0);

	if (status != 1)
		trap_handler(NULL); // TODO : CHANGE!!!

	do_io.commandAddr = command_addr;
	do_io.commandValue = disk_block << 8 | (is_write ? WRITEBLK : READBLK);

	SYSCALL(SENDMESSAGE, (unsigned int)ssi_pcb, (unsigned int)(&payload),
		0);
	SYSCALL(RECEIVEMESSAGE, (unsigned int)ssi_pcb, (unsigned int)(&status),
		0);

	if (status != 1)
		trap_handler(NULL);
}
