#include <iostream>
#include "uthreads.h"
#include "uthread_manager.h"

void thread_test_callback()
{
	std::wcout << L"thread_test_callback" << std::endl;

	uthread_terminate(uthread_get_tid());
}

int main()
{
	uthread_init(100000);
	uthread_manager& manager = global_uthread_manager::get_instance();
	manager.spawn(thread_test_callback);
	std::wcout << L"test" << std::endl;
	return 0;
}
