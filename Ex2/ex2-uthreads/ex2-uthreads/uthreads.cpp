#include <iostream>

#include "uthreads.h"
#include "uthread_manager.h"

int safe_library_call(std::function<void()> func)
{
	try
	{
		func();
		return 0;
	}
	catch (const uthread_exception& e)
	{
		std::cerr << e.what() << std::endl;
	}
	catch (const system_exception& e)
	{
		std::cerr << e.what() << std::endl;
	}
	catch (...)
	{
		std::cerr << "system error: " << std::endl;
	}

	return -1;
}	

int uthread_init(int quantum_usecs)
{
	return safe_library_call([quantum_usecs]
	{
		global_uthread_manager::init(quantum_usecs);
		// Initializing the main thread AFTER the initiailization of the manager
		// to avoid a race condition, in which uthread functions are called before
		// initialization is complete, by other threads
		global_uthread_manager::get_instance().init_main_thread();
	});
}

int uthread_spawn(thread_entry_point entrypoint)
{
	return safe_library_call([entrypoint]
	{
		global_uthread_manager::get_instance().spawn(entrypoint);
	});
}

int uthread_terminate(int tid)
{
	return safe_library_call([tid]
	{
		global_uthread_manager::get_instance().terminate(tid);
	});
}

int uthread_get_tid()
{
	return global_uthread_manager::get_instance().get_tid();
}

