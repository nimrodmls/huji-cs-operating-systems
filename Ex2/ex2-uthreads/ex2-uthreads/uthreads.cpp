#include <iostream>
#include <functional>

#include "uthreads.h"
#include "uthread_manager.h"

int safe_library_call(const std::function<int()>& func)
{
	try
	{
		return func();
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
		return 0;
	});
}

int uthread_spawn(thread_entry_point entrypoint)
{
	return safe_library_call([entrypoint]
	{
		global_uthread_manager::get_instance().spawn(entrypoint);
		return 0;
	});
}

int uthread_terminate(int tid)
{
	return safe_library_call([tid]
	{
		global_uthread_manager::get_instance().terminate(tid);
		return 0;
	});
}

int uthread_block(int tid)
{
	return safe_library_call([tid]
	{
		global_uthread_manager::get_instance().block(tid);
		return 0;
	});
}

int uthread_resume(int tid)
{
	return safe_library_call([tid]
	{
		global_uthread_manager::get_instance().resume(tid);
		return 0;
	});
}

int uthread_sleep(int num_quantums)
{
	return safe_library_call([num_quantums]
	{
		global_uthread_manager::get_instance().sleep(num_quantums);
		return 0;
	});
}

int uthread_get_tid()
{
	return safe_library_call([]
	{
		return global_uthread_manager::get_instance().get_tid();
	});
}

int uthread_get_total_quantums()
{
	return safe_library_call([]
	{
		return global_uthread_manager::get_instance().get_total_elapsed_quantums();
	});
}

int uthread_get_quantums(int tid)
{
	return safe_library_call([tid]
	{
		return global_uthread_manager::get_instance().get_elapsed_quantums(tid);
	});
}
