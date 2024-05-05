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

uthread_manager::uthread_manager(int quantum_usecs) :
	quantum_usecs_interval(quantum_usecs)
{
	// Non-positive quantum_usecs is considered an error
	if (quantum_usecs <= 0)
	{
		throw uthread_exception();
	}
}
