#include "headers/ssi.h"
#include "headers/initial.h"
#include "headers/utils.h"
#include <uriscv/liburiscv.h>

pcb_t *create_process(pcb_t *sender, ssi_create_process_t *p);
static void _terminate_process(pcb_t *sender, pcb_t *p);
static void do_io(pcb_t *sender, ssi_do_io_t *p);
static cpu_t get_cpu_time(pcb_t *p);
static void wait_for_clock(pcb_t *p);
static support_t *get_support_data(pcb_t *p);
static int get_process_id(pcb_t *sender, void *arg);

static void answer_do_io(int device_type, int device_number, int status);
static void answer_wait_for_clock();

LIST_HEAD(pcb_blocked_after_io);

void ssi()
{
	while (1) {
		// Receive a request
		unsigned int payload_tmp;
		unsigned int sender = SYSCALL(RECEIVEMESSAGE, ANYMESSAGE,
					      (unsigned int)(&payload_tmp), 0);

		if (sender == INTERRUPT_HANDLER_MSG) {
			interrupt_handler_io_msg_t msg = {
				.payload = (unsigned int)payload_tmp
			};
			switch (msg.fields.service) {
			case 0: {
				answer_do_io(msg.fields.device_type,
					     msg.fields.device_number,
					     msg.fields.status);
				break;
			}
			case 1: {
				answer_wait_for_clock();
				break;
			}
			default:
				PANIC();
				break;
			}
		} else {
			ssi_payload_t *payload = (ssi_payload_t *)payload_tmp;
			switch (payload->service_code) {
			case CREATEPROCESS: {
				pcb_t *p = create_process(
					(pcb_t *)sender,
					(ssi_create_process_t *)payload->arg);

				unsigned int msg =
					(p == NULL ? NOPROC : (unsigned int)p);

				SYSCALL(SENDMESSAGE, sender, msg, 0);
			} break;

			case TERMPROCESS: {
				_terminate_process((pcb_t *)sender,
						   (pcb_t *)payload->arg);
				SYSCALL(SENDMESSAGE, sender, 0,
					0); // ack ?? If sender is terminated?
				break;
			}

			case DOIO: {
				do_io((pcb_t *)sender,
				      (ssi_do_io_t *)payload->arg);
				// ack is when interrupt_handler send me a message
				break;
			}

			case GETTIME: {
				cpu_t t = get_cpu_time((pcb_t *)sender);
				SYSCALL(SENDMESSAGE, sender, t, 0);
				break;
			}

			case CLOCKWAIT: {
				wait_for_clock((pcb_t *)sender);
				// ack is when interrupt_handler send me a message
				break;
			}

			case GETSUPPORTPTR: {
				support_t *s =
					get_support_data((pcb_t *)sender);
				SYSCALL(SENDMESSAGE, sender, (unsigned int)s,
					0);
				break;
			}

			case GETPROCESSID: {
				int pid = get_process_id((pcb_t *)sender,
							 payload->arg);
				SYSCALL(SENDMESSAGE, sender, pid, 0);
				break;
			}

			default: {
				// If service does not match any of those provided by the SSI,
				// the SSI should terminate the process and its progeny.
				terminate_process((pcb_t *)sender);
				break;
			}
			}
		}
	}
}

pcb_t *create_process(pcb_t *sender, ssi_create_process_t *p)
{
	pcb_t *new_p = allocPcb();
	if (new_p == NULL) {
		return NULL;
	}
	new_p->p_s = *(p->state);
	new_p->p_supportStruct = p->support;

	insertChild(sender, new_p);
	insertProcQ(&ready_queue, new_p);
	process_count++;

	return new_p;
}

static void _terminate_process(pcb_t *sender, pcb_t *p)
{
	// If p does not exist or is not a process?
	if (p == NULL)
		terminate_process(sender);
	else
		terminate_process(p);
}

static void do_io(pcb_t *sender, ssi_do_io_t *p)
{
	int n = comm_add_to_number((unsigned int)p->commandAddr);
	sender->do_io = 1;

	insertProcQForIO(&pcb_blocked_on_device[n], sender);
	*(p->commandAddr) = p->commandValue;
}

static cpu_t get_cpu_time(pcb_t *p)
{
	return p->p_time;
}

static void wait_for_clock(pcb_t *p)
{
	insertProcQForIO(&pcb_blocked_on_clock, p);
}

static support_t *get_support_data(pcb_t *p)
{
	return p->p_supportStruct;
}

static int get_process_id(pcb_t *sender, void *arg)
{
	if (arg == 0) {
		return sender->p_pid;
	} else if (sender->p_parent == NULL) {
		return 0;
	} else {
		return sender->p_parent->p_pid;
	}
}

static void answer_do_io(int device_type, int device_number, int status)
{
	// If the interrupt handler sends me only the device type and
	// is number (so the position in the pcb_blocked_on_device array
	// I can look up the pcb to send the message to and remove him from
	// the list;
	int tmp = hash_from_device_type_number(device_type, device_number);
	pcb_t *dest = removeProcQForIO(&pcb_blocked_on_device[tmp]);
	SYSCALL(SENDMESSAGE, (unsigned int)dest, status, 0);
}

static void answer_wait_for_clock()
{
	pcb_t *dest;
	while ((dest = removeProcQForIO(&pcb_blocked_on_clock))) {
		SYSCALL(SENDMESSAGE, (unsigned int)dest, 0, 0);
	}
}

int is_a_softblocking_request(ssi_payload_t *p)
{
	return p->service_code == DOIO || p->service_code == CLOCKWAIT;
}
