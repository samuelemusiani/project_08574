#include "headers/sst.h"
#include "headers/utils3.h"
#include <uriscv/liburiscv.h>

pcb_PTR sst_pcbs[UPROCMAX];

extern pcb_PTR ssi_pcb;

static void write(sst_print_t *write_payload, unsigned int asid,
		  unsigned int dev)
{
	// dev can only be IL_PRINTER or IL_TERMINAL
	switch (dev) {
	case IL_PRINTER:
		// print a string of caracters to the printer
		// with the same number of the sender ASID (-1)
		int count = 0;
		devreg_t *base = (devreg_t *)DEV_REG_ADDR(dev, asid - 1);
		unsigned int *data0 = &(base->dtp.data0);
		unsigned int *command = &(base->dtp.command);
		unsigned int status;

		while (write_payload->length > count) {
			unsigned int value =
				(unsigned int)*(write_payload->string + count);
			ssi_do_io_t do_io_char = {
				.commandAddr = data0,
				.commandValue = value,
			};
			ssi_payload_t payload_char = {
				.service_code = DOIO,
				.arg = &do_io_char,
			};

			// send char to printer
			SYSCALL(SENDMESSAGE, (unsigned int)ssi_pcb,
				(unsigned int)(&payload_char), 0);

			ssi_do_io_t do_io_command = {
				.commandAddr = command,
				.commandValue = PRINTCHR,
			};
			ssi_payload_t payload_command = {
				.service_code = DOIO,
				.arg = &do_io_command,
			};

			SYSCALL(SENDMESSAGE, (unsigned int)ssi_pcb,
				(unsigned int)(&payload_command), 0);
			SYSCALL(RECEIVEMESSAGE, (unsigned int)ssi_pcb,
				(unsigned int)(&status), 0);
			if ((status & WRITESTATUSMASK) != 1)
				PANIC();

			count++;
		}
		break;
	case IL_TERMINAL:
		// print a string of caracters to the terminal
		// with the same number of the sender ASID (-1)

		base = (devreg_t *)DEV_REG_ADDR(dev, asid - 1);
		command = &(base->term.transm_command);

		count = 0;
		while (write_payload->length > count) {
			unsigned int value =
				PRINTCHR |
				(((unsigned int)*(write_payload->string + count))
				 << 8);
			ssi_do_io_t do_io = {
				.commandAddr = command,
				.commandValue = value,
			};
			ssi_payload_t payload = {
				.service_code = DOIO,
				.arg = &do_io,
			};
			SYSCALL(SENDMESSAGE, (unsigned int)ssi_pcb,
				(unsigned int)(&payload), 0);
			SYSCALL(RECEIVEMESSAGE, (unsigned int)ssi_pcb,
				(unsigned int)(&status), 0);

			if ((status & WRITESTATUSMASK) != RECVD)
				PANIC();

			count++;
		}
		break;
	default:
		PANIC();
	}
}

void sst()
{
	// Get the support structure from the ssi. This is the same for the
	// child process
	ssi_payload_t getsupportdata = { .service_code = GETSUPPORTPTR,
					 .arg = NULL };
	SYSCALL(SENDMESSAGE, (unsigned int)ssi_pcb,
		(unsigned int)(&getsupportdata), 0);
	support_t *support;
	SYSCALL(RECEIVEMESSAGE, (unsigned int)ssi_pcb, (unsigned int)&support,
		0);

	// The asid variable is used to identify which u-proc the current sst
	// need to manage
	int asid = support->sup_asid;

	// SST creates a child process that executes one of the U-proc testers
	// SST shares the same ASID and support structure with its child U-proc.

	state_t child_state;
	STST(&child_state);

	child_state.pc_epc = 0x800000B0;
	child_state.reg_sp = 0xC0000000;

	// not sure about this, I took the values from p2test's processes
	child_state.status |= MSTATUS_MIE_MASK & ~MSTATUS_MPP_M;

	child_state.mie = MIE_ALL;
	pcb_PTR child_pcb = p_create(&child_state, support);

	// After its child initialization, the SST will wait for service
	// requests from its child process
	while (1) {
		ssi_payload_t *payload;
		SYSCALL(RECEIVEMESSAGE, (unsigned int)child_pcb,
			(unsigned int)&payload, 0);

		// Scrivo in italiano perché faccio prima: Le linee seguenti
		// sono commentate perché non realmente necessarie con i test
		// che hanno fornito i tutor. Tuttavia questi test non provano
		// lo swap, che mi sono messo a provare aggiungendo array molto
		// grandy allo stack dei processi di test in modo da forzare
		// l'utilizzo di più pagine. Facendo questo ho scoperto che i
		// test non passavano completamente e venivano printate delle
		// scritte sbagliate su terminale. Ci ho perso 3 o 4 settimane
		// (non voglio pensarci) ma penso di aver trovato il problema.
		//
		// Forzano molto l'uso dello swap, i processi erano
		// continuamente in competizione tra loro e si continuavano a
		// caricare e scaricare pagine a vincenda. Il sitema è quindi in
		// tashing ma non abbiamo nessun controllo per mettere in pausa
		// dei processi e farli continuare più avanti. Nonostante questo
		// il vero problema è il seguente: se un processo di test
		// (uproc) vuole fare una write su termianale, deve chiedere al
		// suo sst corrispettivo, che è suo padre e condivide la stessa
		// struttura di supporto (IMPORTANTE). Data la natura del
		// microkernel, le SYSCALL sono sempre e solo 2: SEND e RECEIVE.
		// Questo purtroppo crea un problema, in quanto non si è mai
		// relamente certi che il processo abbia fatto la RECEIVE prima
		// di processare la SEND. Quindi può capitare che si inizi a
		// processare la SEND prima che la RECEIVE sia stata fatta,
		// ovvero prima che il processo sia bloccato.
		//
		// In questo caso un processo di test (uproc) fa una SEND verso
		// il suo sst per scrivere su terminale. La SEND passa per il
		// gestore delle syscall a livello kernel, e poi viene fatto il
		// pass_up per gestire la syscall a livello di supporto. Il
		// livello di supporto fa un'ulteriore syscall verso il padre
		// del processo (l'sst) e lo risveglia chiedendo una print su
		// terminale. Come stringa da printare viene passato un
		// puntatore che si trova in un indirizzo virtuale del processo
		// di test. La syscall da parte del livello di supporto di
		// conclude, ma essnedo gli interrupt abilitati arriva arriva
		// l'interrupt del CPU-Timer e l'uproc viene messo in coda alla
		// rady_queue (prima o dopo la LDST, però prima della RECEIVE).
		//
		// Degli altri processi eseguono e tolgono la pagina su cui è
		// prensete l'indirizzo della stringa da stampare per l'sst e di
		// conseguenza anche la pagina di stack il processo di test.
		//
		// L'sst parte ad eseguire e deve printare una carattere alla
		// volta. Inizia a leggere la stringa ma l'indirizzo logico non
		// è nel tlb e neanche in memoria. Viene fatta la tlb-refill e
		// successivamente viene chiamato il tlb_handler (pager) per
		// caricare la pagina in memoria. Per chiamare il tlb_handler,
		// tutto lo stato dell'sst viene scritto nella struttura di
		// supporto. Il tlb_handler inizia ad eseguire e arriva a
		// chiedere la mutex per la swap_pool_table. Viene messo in
		// pausa e si passa al prossimo processo.
		//
		// Prima o poi arriverà il processo di test che era stato messo
		// nella ready queue per colpa dell'interrupt di CPU-Timer. Il
		// problema è che per eseguire ha necessità delle sue pagine in
		// memoria, quindi chiamerà anche lui il tlb_handler e *andrà a
		// sovrascrivere lo stato dell'sst* che stava aspettando la
		// mutex. Il processo di test si blocca sulla richiesta per la
		// mutex.
		//
		// Si risveglia l'sst, che però ha i registri sovrascritti e da
		// qui si rompe tutto. Ho notato che delle volte cambia il
		// current_process e dall'sst passa magicamente al figlio. Visto
		// che antrambi i processi sono bloccati sulla mutex, quando uno
		// caricherà effettivamente la pagina in memoria l'altro non se
		// ne sarà accorto e farà lo stesso, crando un duplicato della
		// pagina e rompendo quindi tutta la consistenza dei dati.
		//
		// La soluzione sottostante è orribile: aspettare abbastanza in
		// modo di essere quasi sicuri che il processo figlio abbia
		// fatto la RECEIVE e sia effettivamente bloccato. La solzione
		// più pulita sarebbe fare un SYSCALL che manda un messaggio e
		// blocca il processo in modo atomico. Però non posso mettermi a
		// fare quello che voglio per questo progetto, perché altrimenti
		// lo avrei fatto tutto monolitico :)

		// for (int i = 0; i < 10000; i++)
		//	;

		switch (payload->service_code) {
		case GET_TOD:
			unsigned int tod;
			STCK(tod);
			SYSCALL(SENDMESSAGE, (unsigned int)child_pcb, tod, 0);
			break;
		case TERMINATE:
			p_term(child_pcb);
			SYSCALL(SENDMESSAGE, PARENT, 0, 0);
			p_term(SELF);
			break;
		case WRITEPRINTER:
			sst_print_t *print_payload =
				(sst_print_t *)payload->arg;
			write(print_payload, asid, IL_PRINTER);
			SYSCALL(SENDMESSAGE, (unsigned int)child_pcb, 0, 0);
			break;
		case WRITETERMINAL:
			sst_print_t *term_payload = (sst_print_t *)payload->arg;
			write(term_payload, asid, IL_TERMINAL);
			SYSCALL(SENDMESSAGE, (unsigned int)child_pcb, 0, 0);
			break;
		}
	}
}
