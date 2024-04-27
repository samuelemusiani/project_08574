#include "headers/sst.h"
#include "headers/utils3.h"
#include <uriscv/liburiscv.h>

pcb_PTR sst_pcbs[UPROCMAX];

extern pcb_PTR test_pcb; // WE SHOULD NOT DO THIS. I think the cleanest way is
			 // to make syscalls on nucleus accept 0 as the parent
			 // sender

void sst()
{
	// The asid variable is used to identify which u-proc the current sst
	// need to managed.
	unsigned int asid = GET_ASID;
	asid++; // DELETE, need to compile :)

	// SST create a child process that executes one of the test U-pr

	// SST shares the same ASID and support structure with its child U-proc.

	// After its child initialization, the SST will wait for service
	// requests from its child process
	//
	// while(true) {
	// ...
	// }

	// TODO: DO NOT USE test_pcb
	SYSCALL(SENDMESSAGE, (unsigned int)test_pcb, 0, 0);
	p_term(SELF);
}
