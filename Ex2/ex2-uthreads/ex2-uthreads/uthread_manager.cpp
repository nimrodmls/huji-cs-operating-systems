#include <iostream>
#include <signal.h>
#include <sys/time.h>

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
		running_thread = threads.at(MAIN_THREAD_ID);
	}
	catch (const std::bad_alloc&)
	{
		throw system_exception("thread allocation failed");
	}

	// Setting up the timer - CRITICAL PLACEMENT OF THIS CODE
	struct sigaction new_action = { 0 };
	new_action.sa_handler = sigvtalrm_handler;
	sigemptyset(&new_action.sa_mask);
	sigaddset(&new_action.sa_mask, SIGVTALRM);
	sigaction(SIGVTALRM, &new_action, NULL);

	struct itimerval timer = { 0 };
	timer.it_value.tv_usec = quantum_usecs_interval;
	timer.it_interval.tv_usec = quantum_usecs_interval;

	if (-1 == setitimer(ITIMER_VIRTUAL, &timer, NULL))
	{
		throw system_exception("timer setup failed");
	}
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
		ready_threads.push_back(tid);
		env[tid][0] = threads.at(tid)->get_env_block()[0];
	}
	catch (const std::bad_alloc&)
	{
		throw system_exception("thread allocation failed");
	}
}

void uthread_manager::terminate(thread_id tid)
{
	// If the requested thread to terminate is the main thread, we should exit the program
	// as specified in the exercise.
	if (MAIN_THREAD_ID == tid)
	{
		exit(0);
	}

	const auto thread = threads.find(tid);
	if (thread == threads.end())
	{
		throw uthread_exception("thread id not found");
	}

	// If the thread is running, we should switch to the next thread and erase this one
	if (RUNNING == thread->second->get_state())
	{
		switch_threads(false, true);
	}

	// If the thread is ready, we should erase it from the ready queue
	if (READY == thread->second->get_state())
	{
		const auto find_result =
			std::find(ready_threads.begin(), ready_threads.end(), thread->second->get_id());
		if (find_result != ready_threads.end())
		{
			ready_threads.erase(find_result);
		}
	}

	// We can safely do this here since we will reach here only if the thread is READY or BLOCKED
	// (because on RUNNING, we will switch to another thread and never return here)
	threads.erase(tid);
}

void uthread_manager::block(thread_id tid)
{
	if (MAIN_THREAD_ID == tid)
	{
		throw uthread_exception("cannot block the main thread");
	}

	if (tid == running_thread->get_id())
	{
		// The running thread is blocking itself, we should switch to the next thread
		switch_threads(true, false);
	}
	else
	{
		// Looking up the thread
		const auto thread = threads.find(tid);
		if (thread == threads.end())
		{
			throw uthread_exception("thread id not found");
		}
		// If the thread was found, looking for it in the ready queue and erasing it
		const auto find_result = 
			std::find(ready_threads.begin(), ready_threads.end(), thread->second->get_id());
		if (find_result != ready_threads.end())
		{
			ready_threads.erase(find_result);
			threads.at(*find_result)->set_state(BLOCKED);
		}
	}
}

void uthread_manager::resume(thread_id tid)
{
	// Looking up the thread
	const auto thread = threads.find(tid);
	if (thread == threads.end())
	{
		throw uthread_exception("thread id not found");
	}

	// We resume the thread only if it is blocked and its sleep time has passed
	// Therefore, resuming READY and RUNNING threads, or BLOCKED threads with
	// remaining sleep time is not an error, and simply ignored.
	if ((BLOCKED == thread->second->get_state()) && 
		(0 == thread->second->get_sleep_time()))
	{
		ready_threads.push_back(thread->second->get_id());
	}
}

void uthread_manager::sleep(int sleep_quantums)
{
	if (MAIN_THREAD_ID == running_thread->get_id())
	{
		throw uthread_exception("cannot sleep the main thread");
	}

	// Putting the thread to sleep
	running_thread->set_sleep_time(sleep_quantums);
	// TODO: Sleep time updating
	// Switching to the next thread
	switch_threads(true, false);
}

void uthread_manager::switch_threads(bool is_blocked, bool terminate_running)
{
	std::cout << "switching from thread " << running_thread->get_id() << std::endl;
	//const int paused = running_thread->suspend(is_blocked);
	const int paused = sigsetjmp(env[running_thread->get_id()], 1);
	// The thread has been paused, we should switch to the next thread
	// in the ready queue
	if (pause_state::SUSPENDED == paused)
	{
		// Pushing the paused thread to the end of the queue, only if the switching
		// was not triggered by a block
		if (!is_blocked)
		{
			running_thread->set_state(READY);
			ready_threads.push_back(running_thread->get_id()); 
		}
		else
		{
			running_thread->set_state(BLOCKED);
		}

		// If the thread is terminating, we should remove it from the threads map
		if (terminate_running)
		{
			threads.erase(running_thread->get_id());
		}
		
		running_thread = threads.at(ready_threads.front()); // Getting the next thread to run
		ready_threads.pop_front(); // Removing the thread from the queue

		std::cout << "switching to thread " << running_thread->get_id() << std::endl;
		//running_thread->run(); // Running the thread
		siglongjmp(env[running_thread->get_id()], RESUMED);
	}
	else
	{
		running_thread->set_state(RUNNING);
	}
	// otherwise the thread has been resumed, and we should continue running it.
	// Updating the running thead would not be necessary since we already do so
	// in the suspend handling.
}

thread_id uthread_manager::get_tid() const
{
	return running_thread->get_id();
}

int uthread_manager::get_elapsed_quantums(thread_id tid) const
{
	const auto thread = threads.find(tid);
	if (thread == threads.end())
	{
		throw uthread_exception("thread id not found");
	}

	return thread->second->get_elapsed_quantums();
}

void uthread_manager::sigvtalrm_handler(int sig_num)
{
	// omitting sig_num because this handler should be called only by SIGVTALRM
	// Switching the currently active thread with the next ready thread
	//auto mutex = ctx_switch_mutex();
	std::cout << "sigvtalrm_handler call" << global_uthread_manager::get_instance().get_tid() << std::endl;
	global_uthread_manager::get_instance().switch_threads(false, false);
	global_uthread_manager::get_instance().elapsed_quantums++;
	global_uthread_manager::get_instance().running_thread->increment_elapsed_quantums();
	std::cout << "sigvtalrm_handler done" << global_uthread_manager::get_instance().get_tid() << std::endl;
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