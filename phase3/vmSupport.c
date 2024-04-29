#include "headers/vmSupport.h"
#include "headers/utils3.h"

#define NOPROC -1

swap_t *swap_pool;

void initSwapStructs()
{
	// Init swap pool table
	swap_pool = (swap_t *)FLASHPOOLSTART; // TODO: Find more precise value

	for (int i = 0; i < POOLSIZE; i++) {
		swap_pool[i].sw_asid = NOPROC;
	}

	swap_pool_mutex.asid = -1;
	swap_pool_mutex.sem = 1;
}

// Processo mutex
void mutex_proc()
{
	// TODO: Mutex process
	p_term(SELF);
}

void tlb_handler()
{
}




extern pcb_t *ssi_pcb;
#define SWAP_POOL_SIZE (2*UPROCMAX)
typedef struct support_mutex {
  int sem; // value of the semaphore
  int asid; // asid of the process that has the mutex
} support_mutex;

// La swap pool table è un array di swap_t entries
swap_t swap_pool_table[SWAP_POOL_SIZE];


support_mutex swap_pool_mutex;

void pager(){
	// Devo mandare una SYSCALL a SSI per richieddere il getsupportdata
	// Devo ricevere il messaggio da SSI
	ssi_payload_t getsupportdata = {
		.service_code = GETSUPPORTPTR
	};
	
	
	SYSCALL(SENDMESSAGE, (unsigned int)ssi_pcb, (unsigned int)(&getsupportdata), 0);
	support_t *s = (support_t *)SYSCALL(RECEIVEMESSAGE, (unsigned int)ssi_pcb, 0, 0);

	// Se la causa è una TLB-Modification exception, deve considerare l'eccezione come program trap
	// ovvero cause register ha excode 24
	unsigned int tlbcause = s->sup_exceptState[PGFAULTEXCEPT].cause;
	unsigned int excCode = tlbcause & GETEXECCODE;

	// è 24?? leggendo NOTES mi sembra di si 
	if (excCode == 24) {
		// TLB-Modification exception is treated as a program trap
		support_program_trap_handler();
	}
	else { // Se la causa è una TLB-invalid
	

	// devo prendere la mutua esclusione sulla swap pool table mandando un messaggio allo swap mutex
	// e ricevendo il messaggio di conferma
	SYSCALL(SENDMESSAGE, (unsigned int)mutex_pcb, 0, 0);
	SYSCALL(RECEIVEMESSAGE, (unsigned int)mutex_pcb, 0, 0);



	// Devo disabilitare gli interrupt
	swap_pool_mutex.sem -= 1;
	swap_pool_mutex.asid = 

	unsigned int p = s->sup_exceptState[0].entry_hi;

	// scegliere un frame i di ram dallo swap pool (FIFO) 

	// se il frame i è occupato da una pagina logica k del processo x e la pagina k è dirty (modificata)
	// allora
	// 	- aggiorno la page table del processo x (in mutua esclusione)
	//  - aggiorno la TLB, se la pagina k del processo x è in TLB (in mutua esclusione)
	//  - scrivo il contenuto del frame i sulla posizione correta del flash device del processo x

	// swap pool starting address is 0x20000000 + (32 * PAGESIZE)

	static memaddr swap_pool = 0x20000000 + (32 * PAGESIZE);

	if (swap_pool_table[i].sw_pte != NULL) {
		
		// controllo se la pagina è dirty
		unsigned int dirty = swap_pool_table[i].sw_pte->pte_entryLO & DIRTYON;
		if (dirty) {

			// richiedo l'accesso in mutua esclusione alla swap pool table
			SYSCALL(SENDMESSAGE, (unsigned int)mutex_pcb, (unsigned int)(1), 0);
			unsigned int ack = SYSCALL(RECEIVEMESSAGE, (unsigned int)mutex_pcb, 0, 0);

			// marco la page table entry k del processo x come non valida
			swap_pool_table[i].sw_pte->pte_entryLO = swap_pool_table[i].sw_pte->pte_entryLO & ~VALIDON;

			// se la entry k del processo x è in TLB, cancello tutte le entry della TLB
			TLBCLR();

			// aggiorno la page table del processo x
			unsigned int x = swap_pool_table[i].sw_pte->pte_entryHI & GETVPN;
			unsigned int k = swap_pool_table[i].sw_pte->pte_entryLO & GETPFN;
			unsigned int i = swap_pool_table[i].sw_pte->pte_entryLO & GETFRAME;
		
			// aggiorno la page table del processo x (in mutua esclusione)
			SYSCALL(SENDMESSAGE, (unsigned int)mutex_pcb, (unsigned int)(&getsupportdata), 0);
			SYSCALL(RECEIVEMESSAGE, (unsigned int)mutex_pcb, 0, 0);
			// aggiorno la TLB, se la pagina k del processo x è in TLB (in mutua esclusione)
			SYSCALL(SENDMESSAGE, (unsigned int)tlb_pcb, (unsigned int)(&getsupportdata), 0);
			SYSCALL(RECEIVEMESSAGE, (unsigned int)tlb_pcb, 0, 0);
			// scrivo il contenuto del frame i sulla posizione corretta del flash device del processo x
			SYSCALL(SENDMESSAGE, (unsigned int)flash_pcb, (unsigned int)(&getsupportdata), 0);
			SYSCALL(RECEIVEMESSAGE, (unsigned int)flash_pcb, 0, 0);
		}

		

	}
	}
}


int isvalidAsid(int asid)
{
	return asid > 0 && asid <= UPROCMAX;
}

int isFrameOccupied(swap_t *frame)
{
	return frame->sw_asid != -1;
}

// Convert page_index [0, 16] to the corresponding address in the swap pool
memaddr page_index_address(size_tt page_index)
{
	if (page_index < 0 || page_index >= SWAP_POOL_SIZE) {
		return (memaddr)NULL;
	}
	else {
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
		// To modify the content of the TLB, we need to write value into the CP0 registers
		setENTRYHI(pte->pte_entryHI);
		setENTRYLO(pte->pte_entryLO);
		// TLB write index command that writes the entry in the correct TLB index
		TLBWI();
	}
}

// PandOS replacement algorithm round robin
size_tt getFrameIndex()
{
	// Cerco un frame libero
	for (size_tt i = 0; i < SWAP_POOL_SIZE; i++) {
		if (!isFrameOccupied(&swap_pool_table[i])) {
			return i;
		}
	}

	// Se non ci sono frame liberi, uso la politica FIFO
	static size_tt frame_index = -1;
	frame_index = (frame_index + 1) % SWAP_POOL_SIZE;
	return frame_index;
}

