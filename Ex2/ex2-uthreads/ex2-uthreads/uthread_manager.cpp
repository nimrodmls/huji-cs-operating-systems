#include <iostream>
#include "uthread_manager.h"

void uthread_manager::init(int quantum_usecs)
{
	try
	{
		instance = std::make_shared<uthread_manager>(quantum_usecs);
	}
	catch (const std::bad_alloc&)
	{
		std::cerr << "system error: memory allocation failed" << std::endl;
		throw uthread_exception();
	}
}

// In this exercise we will use get_instance without checking if instance is nullptr
// since the exercise description states that the instance will be initialized before
// any other function is called.
std::shared_ptr<uthread_manager> uthread_manager::get_instance()
{
	return instance;
}

void uthread_manager::spawn(thread_entry_point entry_point)
{

}

uthread_manager::uthread_manager(int quantum_usecs) :
	quantum_usecs_interval(quantum_usecs)
{
	// Non-positive quantum_usecs is considered an error
	if (quantum_usecs <= 0)
	{
		throw uthread_exception();
	}
}

void global_uthread_manager::init(int quantum_usecs)
{
	get_instance(quantum_usecs);
}

std::shared_ptr<uthread_manager> global_uthread_manager::get_instance()
{
	// Giving out some random value since the instance
	// should be instantiated beforehand with a real value
	return get_instance(0);
}

std::shared_ptr<uthread_manager> global_uthread_manager::get_instance(int quantum_usecs)
{
	static std::shared_ptr<uthread_manager> instance(quantum_usecs);
	return instance;
}