#include "uthreads.h"
#include "uthread_manager.h"
#include <setjmp.h>

int uthread_init(int quantum_usecs)
{
	try
	{
		global_uthread_manager::init(quantum_usecs);
		// Initializing the main thread AFTER the initiailization of the manager
		// to avoid a race condition, in which uthread functions are called before
		// initialization is complete, by other threads
		global_uthread_manager::get_instance().init_main_thread();
	}
	catch (const uthread_exception&)
	{
		return -1;
	}

	return 0;
}

int uthread_spawn(thread_entry_point entrypoint)
{
	try
	{
		global_uthread_manager::get_instance().spawn(entrypoint);
	}
	catch (const uthread_exception&)
	{
		return -1;
	}

	return 0;
}

int uthread_terminate(int tid)
{
	try
	{
		global_uthread_manager::get_instance().terminate(tid);
	}
	catch (const uthread_exception&)
	{
		return -1;
	}

	return 0;
}

int uthread_get_tid()
{
	return global_uthread_manager::get_instance().get_tid();
}

