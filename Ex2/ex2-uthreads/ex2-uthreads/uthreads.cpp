#include "uthreads.h"
#include "uthread_manager.h"
#include <setjmp.h>

int uthread_init(int quantum_usecs)
{
	try
	{
		uthread_manager::init(quantum_usecs);
	}
	catch (const uthread_exception&)
	{
		return -1;
	}

	return 0;
}


