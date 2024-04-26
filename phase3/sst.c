#include "headers/sst.h"

void sst()
{
	// The asid variable is used to identify which u-proc the current sst
	// need to managed.
	unsigned int asid = getENTRYHI();
	asid++; // DELETE, need to compile :)

	// SST create a child process that executes one of the test U-pr

	// SST shares the same ASID and support structure with its child U-proc.

	// After its child initialization, the SST will wait for service
	// requests from its child process
	//
	// while(true) {
	// ...
	// }
}
