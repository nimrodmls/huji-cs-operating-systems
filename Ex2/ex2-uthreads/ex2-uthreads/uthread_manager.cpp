#include <signal.h>

#include "uthread_manager.h"

uthread_manager::uthread_manager(int quantum_usecs) :
	quantum_usecs_interval(quantum_usecs),
	elapsed_quantums(1)
{
	// Non-positive quantum_usecs is considered an error
	if (quantum_usecs <= 0)
	{
		throw uthread_exception("invalid quantum interval value");
	}
}

void uthread_manager::init_main_thread()
{
	try
	{
		threads.emplace(MAIN_THREAD_ID, std::make_shared<thread>(MAIN_THREAD_ID));
	}
	catch (const std::bad_alloc&)
	{
		throw system_exception("thread allocation failed");
	}

	struct sigaction new_action = { 0 };
	new_action.sa_handler = sigvtalrm_handler;
	sigaction(SIGVTALRM, &new_action, NULL);
}

void uthread_manager::spawn(thread_entry_point entry_point)
{
	if (MAX_THREAD_NUM <= threads.size())
	{
		throw uthread_exception("maximum number of threads reached");
	}

	unsigned int tid = threads.size(); // TODO: Better way to get a unique id
	try
	{
		threads.emplace(tid, std::make_shared<thread>(tid, entry_point));
	}
	catch (const std::bad_alloc&)
	{
		throw system_exception("thread allocation failed");
	}
}

void uthread_manager::terminate(thread_id tid)
{
	auto thread = threads.find(tid);
	if (thread == threads.end())
	{
		throw uthread_exception("thread id not found");
	}

	threads.erase(thread);
}

void uthread_manager::block(thread_id tid)
{
}

void uthread_manager::resume(thread_id tid)
{
}

void uthread_manager::sleep(thread_id tid)
{
}

void uthread_manager::switch_threads(bool is_blocked)
{
	int paused = running_thread->pause();
	// Checking if the thread has been successfully paused
	if (pause_state::PAUSED == paused)
	{
		// The thread has been paused, we should switch to the next thread
		// in the ready queue

		if (!is_blocked)
		{
			// Pushing the paused thread to the end of the queue
			ready_threads.push(running_thread); 
		}
		
		running_thread = ready_threads.front(); // Getting the next thread to run
		ready_threads.pop(); // Removing the thread from the queue
		running_thread->run(); // Running the thread
	}
	// otherwise the thread has been resumed, and we should continue running it
}

thread_id uthread_manager::get_tid() const
{
	return running_thread->get_id();
}

int uthread_manager::get_elapsed_quantums() const
{
	return elapsed_quantums;
}

void uthread_manager::sigvtalrm_handler(int sig_num)
{
	// omitting sig_num because this handler should be called only by SIGVTALRM
	// Switching the currently active thread with the next ready thread
	// 
	global_uthread_manager::get_instance().switch_threads(false);
	global_uthread_manager::get_instance().increment_elapsed_quantums();
}

void global_uthread_manager::init(int quantum_usecs)
{
	get_instance(quantum_usecs);
}

uthread_manager& global_uthread_manager::get_instance()
{
	// Giving out some random value since the instance
	// should be instantiated beforehand with a real value
	// we make no further checks because the exercise allows us to
	// assume that uthread's init is always called first.
	return get_instance(0);
}

uthread_manager& global_uthread_manager::get_instance(int quantum_usecs)
{
	// Note: This allows the instantiation of the instance only once
	// and the instance is destroyed when the program ends. This is okay,
	// since the exercise allows us to assume so.
	static uthread_manager instance(quantum_usecs);
	return instance;
}