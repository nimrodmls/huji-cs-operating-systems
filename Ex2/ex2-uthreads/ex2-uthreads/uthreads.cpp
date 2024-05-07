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
		std::cout << "uthread_init call" << std::endl;
		global_uthread_manager::init(quantum_usecs);
		// Initializing the main thread AFTER the initiailization of the manager
		// to avoid a race condition, in which uthread functions are called before
		// initialization is complete, by other threads
		global_uthread_manager::get_instance().init_main_thread();
		std::cout << "uthread_init done" << std::endl;
		return 0;
	});
}

int uthread_spawn(thread_entry_point entrypoint)
{
	auto mutex = ctx_switch_mutex();
	return safe_library_call([entrypoint]
	{
		std::cout << "uthread_spawn call" << global_uthread_manager::get_instance().get_tid() << std::endl;
		global_uthread_manager::get_instance().spawn(entrypoint);
		std::cout << "uthread_spawn done" << global_uthread_manager::get_instance().get_tid() << std::endl;
		return 0;
	});
}

int uthread_terminate(int tid)
{
	auto mutex = ctx_switch_mutex();
	return safe_library_call([tid]
	{
		std::cout << "uthread_terminate call" << global_uthread_manager::get_instance().get_tid() << std::endl;
		global_uthread_manager::get_instance().terminate(tid);
		std::cout << "uthread_terminate done" << global_uthread_manager::get_instance().get_tid() << std::endl;
		return 0;
	});
}

int uthread_block(int tid)
{
	auto mutex = ctx_switch_mutex();
	return safe_library_call([tid]
	{
		std::cout << "uthread_block call" << global_uthread_manager::get_instance().get_tid() << std::endl;
		global_uthread_manager::get_instance().block(tid);
		std::cout << "uthread_block done" << global_uthread_manager::get_instance().get_tid() << std::endl;
		return 0;
	});
}

int uthread_resume(int tid)
{
	auto mutex = ctx_switch_mutex();
	return safe_library_call([tid]
	{
		std::cout << "uthread_resume call" << global_uthread_manager::get_instance().get_tid() << std::endl;
		global_uthread_manager::get_instance().resume(tid);
		std::cout << "uthread_resume done" << global_uthread_manager::get_instance().get_tid() << std::endl;
		return 0;
	});
}

int uthread_sleep(int num_quantums)
{
	auto mutex = ctx_switch_mutex();
	return safe_library_call([num_quantums]
	{
		std::cout << "uthread_sleep call" << global_uthread_manager::get_instance().get_tid() << std::endl;
		global_uthread_manager::get_instance().sleep(num_quantums);
		std::cout << "uthread_sleep done" << global_uthread_manager::get_instance().get_tid() << std::endl;
		return 0;
	});
}

int uthread_get_tid()
{
	auto mutex = ctx_switch_mutex();
	return safe_library_call([]
	{
		return global_uthread_manager::get_instance().get_tid();
	});
}

int uthread_get_total_quantums()
{
	auto mutex = ctx_switch_mutex();
	return safe_library_call([]
	{
		std::cout << "uthread_get_total_quantums call" << global_uthread_manager::get_instance().get_tid() << std::endl;
		auto result = global_uthread_manager::get_instance().get_total_elapsed_quantums();
		std::cout << "uthread_get_total_quantums done" << global_uthread_manager::get_instance().get_tid() << std::endl;
		return result;
	});
}

int uthread_get_quantums(int tid)
{
	auto mutex = ctx_switch_mutex();
	return safe_library_call([tid]
	{
		std::cout << "uthread_get_quantums call" << global_uthread_manager::get_instance().get_tid() << std::endl;
		auto result = global_uthread_manager::get_instance().get_elapsed_quantums(tid);
		std::cout << "uthread_get_quantums done" << global_uthread_manager::get_instance().get_tid() << std::endl;
		return result;
	});
}
