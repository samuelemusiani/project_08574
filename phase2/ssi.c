#include "headers/ssi.h"
#include "headers/initial.h"
#include <uriscv/liburiscv.h>

static pcb_t *create_process(ssi_create_process_t *p);
static void
_terminate_process(pcb_t *p); // Should we move the utils function here?
static void do_io(ssi_do_io_t *p);
static cpu_t get_cpu_time(pcb_t *p);
static void wait_for_clock(pcb_t *p);
static support_t *get_support_data(pcb_t *p);
static int get_process_id(pcb_t *p);

void ssi()
{
	while (1) {
		// Receive a request
		ssi_payload_t *payload;
		unsigned int sender = SYSCALL(RECEIVEMESSAGE, ANYMESSAGE,
					      (unsigned int)(&payload), 0);

		switch (payload->service_code) {
		case CREATEPROCESS: {
			pcb_t *p = create_process(
				(ssi_create_process_t *)payload->arg);
			// Send back p to the sender
			break;
		}

		case TERMPROCESS: {
			_terminate_process((pcb_t *)payload->arg);
			// ack the sender of the termination. When I kill the sender?
			break;
		}

		case DOIO: {
			do_io((ssi_do_io_t *)payload->arg);
			// send respons
			break;
		}

		case GETTIME: {
			cpu_t t = get_cpu_time((pcb_t *)sender);
			// send reponds
			break;
		}

		case CLOCKWAIT: {
			wait_for_clock((pcb_t *)sender);
			// ack the sender ?
			break;
		}

		case GETSUPPORTPTR: {
			support_t *s = get_support_data((pcb_t *)sender);
			// return support data to sender
			break;
		}

		case GETPROCESSID: {
			int pid = get_process_id((pcb_t *)sender);

			break;
		}

		default: {
			// If service does not match any of those provided by the SSI, the SSI
			// should terminate the process and its progeny.
			break;
		}
		}

		// Send back results
	}
}

pcb_t *create_process(ssi_create_process_t *p)
{
	return NULL;
}

static void _terminate_process(pcb_t *p)
{
}

static void do_io(ssi_do_io_t *p)
{
}

static cpu_t get_cpu_time(pcb_t *p)
{
	return 0;
}

static void wait_for_clock(pcb_t *p)
{
}

static support_t *get_support_data(pcb_t *p)
{
	return NULL;
}

static int get_process_id(pcb_t *p)
{
	return 0;
}
