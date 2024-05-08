#include <iostream>
#include <functional>
#include <signal.h>
#include <sys/time.h>

#include "uthreads.h"
#include "uthread_manager.h"

#define MAIN_THREAD_ID 0

/* RAII class for disabling and enabling context switching */
class ctx_switch_mutex
{
public:
	ctx_switch_mutex() { disable_ctx_switch(); }
	~ctx_switch_mutex() { enable_ctx_switch(); }

	ctx_switch_mutex(const ctx_switch_mutex&) = delete;
	ctx_switch_mutex operator=(const ctx_switch_mutex&) = delete;

private:
	static void disable_ctx_switch()
	{
		sigset_t sigset;
		sigemptyset(&sigset);
		sigaddset(&sigset, SIGVTALRM);
		sigprocmask(SIG_BLOCK, &sigset, nullptr);
	}

	static void enable_ctx_switch()
	{
		sigset_t sigset;
		sigemptyset(&sigset);
		sigaddset(&sigset, SIGVTALRM);
		sigprocmask(SIG_UNBLOCK, &sigset, nullptr);
	}
};

/* Context manager for the user threads */
class uthread_mgr
{
public:
	~uthread_mgr() = default;

	uthread_mgr(const uthread_mgr&) = delete;
	uthread_mgr operator=(const uthread_mgr&) = delete;

	static uthread_mgr& get_instance()
	{
		static uthread_mgr instance;
		return instance;
	}

	int quantum_usecs_interval;
	int elapsed_quantums;
	// Storing all the threads that are ready to run, as a queue,
	// the threads will be run in a FIFO order
	std::deque<thread_id> ready_threads;
	// The current running thread
	std::shared_ptr<thread> running_thread;
	// Storing all the active threads (on all states)
	std::map<thread_id, std::shared_ptr<thread>> threads;

private:
	uthread_mgr() :
		quantum_usecs_interval(0), elapsed_quantums(1)
	{}
};

static void print_library_error(const std::string& msg)
{
	std::cerr << "thread library error: " << msg << std::endl;
}

static void print_system_error(const std::string& msg)
{
	std::cerr << "system error: " << msg << std::endl;
}

static void switch_threads(bool is_blocked, bool terminate_running)
{
	uthread_mgr& mgr = uthread_mgr::get_instance();
	std::cout << "Switching from " << mgr.running_thread->get_id() << std::endl;
	//const int paused = running_thread->suspend(is_blocked);
	const int paused = sigsetjmp(mgr.running_thread->env_blk, 1);
	// The thread has been paused, we should switch to the next thread
	// in the ready queue
	if (pause_state::SUSPENDED == paused)
	{
		// Pushing the paused thread to the end of the queue, only if the switching
		// was not triggered by a block
		if (!is_blocked)
		{
			mgr.running_thread->set_state(READY);
			mgr.ready_threads.push_back(mgr.running_thread->get_id());
		}
		else
		{
			mgr.running_thread->set_state(BLOCKED);
		}

		// If the thread is terminating, we should remove it from the threads map
		if (terminate_running)
		{
			mgr.threads.erase(mgr.running_thread->get_id());
		}

		mgr.running_thread = mgr.threads.at(mgr.ready_threads.front()); // Getting the next thread to run
		mgr.ready_threads.pop_front(); // Removing the thread from the queue

		std::cout << "Switching to " << mgr.running_thread->get_id() << std::endl;
		siglongjmp(mgr.running_thread->env_blk, RESUMED);
	}
	else
	{
		mgr.running_thread->set_state(RUNNING);
	}
	// otherwise the thread has been resumed, and we should continue running it.
	// Updating the running thead would not be necessary since we already do so
	// in the suspend handling.
}

static void sigvtalrm_handler(int sig_num)
{
	switch_threads(false, false);
	uthread_mgr::get_instance().elapsed_quantums++;
	uthread_mgr::get_instance().running_thread->increment_elapsed_quantums();
}

int uthread_init(int quantum_usecs)
{
	// Non-positive quantum_usecs is considered an error
	if (quantum_usecs <= 0)
	{
		print_library_error("invalid quantum interval value");
		return -1;
	}

	uthread_mgr::get_instance().quantum_usecs_interval = quantum_usecs;

	// Setting up the main thread
	std::shared_ptr<thread> main_thread = nullptr;
	try
	{
		main_thread = std::make_shared<thread>(MAIN_THREAD_ID);
	}
	catch (const std::bad_alloc&)
	{
		print_system_error("thread allocation failed");
		return -1;
	}
	uthread_mgr::get_instance().threads.emplace(MAIN_THREAD_ID, main_thread);
	uthread_mgr::get_instance().running_thread = main_thread;

	// Setting up the sigaction associated with the timer
	struct sigaction new_action = { 0 };
	new_action.sa_handler = sigvtalrm_handler;
	sigemptyset(&new_action.sa_mask);
	sigaddset(&new_action.sa_mask, SIGVTALRM);
	sigaction(SIGVTALRM, &new_action, NULL);

	// Setting up the timer
	struct itimerval timer = { 0 };
	timer.it_value.tv_usec = uthread_mgr::get_instance().quantum_usecs_interval;
	timer.it_interval.tv_usec = uthread_mgr::get_instance().quantum_usecs_interval;

	if (-1 == setitimer(ITIMER_VIRTUAL, &timer, NULL))
	{
		print_system_error("timer setup failed");
		return -1;
	}

	return 0;
}

int uthread_spawn(thread_entry_point entrypoint)
{
	auto mutex = ctx_switch_mutex();
	if (MAX_THREAD_NUM <= uthread_mgr::get_instance().threads.size())
	{
		print_library_error("maximum number of threads reached");
		return -1;
	}

	unsigned int tid = uthread_mgr::get_instance().threads.size(); // TODO: Better way to get a unique id

	// Creating the new thread
	std::shared_ptr<thread> new_thread = nullptr;
	try
	{
		new_thread = std::make_shared<thread>(tid, entrypoint);
		
	}
	catch (const std::bad_alloc&)
	{
		print_system_error("thread allocation failed");
		return -1;
	}
	// Mapping the new thread and marking it ready
	uthread_mgr::get_instance().threads.emplace(tid, new_thread);
	uthread_mgr::get_instance().ready_threads.push_back(tid);

	return 0;
}

int uthread_terminate(int tid)
{
	auto mutex = ctx_switch_mutex();
	// If the requested thread to terminate is the main thread, we should exit the program
	// as specified in the exercise.
	if (MAIN_THREAD_ID == tid)
	{
		exit(0);
	}

	const auto thread = uthread_mgr::get_instance().threads.find(tid);
	if (thread == uthread_mgr::get_instance().threads.end())
	{
		print_library_error("thread id not found");
		return -1;
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
			std::find(uthread_mgr::get_instance().ready_threads.begin(), 
					  uthread_mgr::get_instance().ready_threads.end(), 
					  thread->second->get_id());
		if (find_result != uthread_mgr::get_instance().ready_threads.end())
		{
			uthread_mgr::get_instance().ready_threads.erase(find_result);
		}
	}

	// We can safely do this here since we will reach here only if the thread is READY or BLOCKED
	// (because on RUNNING, we will switch to another thread and never return here)
	uthread_mgr::get_instance().threads.erase(tid);

	return 0;
}

int uthread_block(int tid)
{
	auto mutex = ctx_switch_mutex();
	uthread_mgr& mgr = uthread_mgr::get_instance();
	if (MAIN_THREAD_ID == tid)
	{
		print_library_error("cannot block the main thread");
		return -1;
	}

	if (tid == mgr.running_thread->get_id())
	{
		// The running thread is blocking itself, we should switch to the next thread
		switch_threads(true, false);
	}
	else
	{
		// Looking up the thread
		const auto thread = mgr.threads.find(tid);
		if (thread == mgr.threads.end())
		{
			print_library_error("thread id not found");
			return -1;
		}
		// If the thread was found, looking for it in the ready queue and erasing it
		const auto find_result =
			std::find(mgr.ready_threads.begin(), mgr.ready_threads.end(), thread->second->get_id());
		if (find_result != mgr.ready_threads.end())
		{
			mgr.ready_threads.erase(find_result);
			mgr.threads.at(*find_result)->set_state(BLOCKED);
		}
	}

	return 0;
}

int uthread_resume(int tid)
{
	auto mutex = ctx_switch_mutex();
	uthread_mgr& mgr = uthread_mgr::get_instance();
	// Looking up the thread
	const auto thread = mgr.threads.find(tid);
	if (thread == mgr.threads.end())
	{
		print_library_error("thread id not found");
		return -1;
	}

	// We resume the thread only if it is blocked and its sleep time has passed
	// Therefore, resuming READY and RUNNING threads, or BLOCKED threads with
	// remaining sleep time is not an error, and simply ignored.
	if ((BLOCKED == thread->second->get_state()) &&
		(0 == thread->second->get_sleep_time()))
	{
		mgr.ready_threads.push_back(thread->second->get_id());
	}

	return 0;
}

int uthread_sleep(int num_quantums)
{
	auto mutex = ctx_switch_mutex();
	uthread_mgr& mgr = uthread_mgr::get_instance();
	if (MAIN_THREAD_ID == mgr.running_thread->get_id())
	{
		print_library_error("cannot sleep the main thread");
		return -1;
	}

	// Putting the thread to sleep
	mgr.running_thread->set_sleep_time(num_quantums);
	// TODO: Sleep time updating
	// Switching to the next thread
	switch_threads(true, false);

	return 0;
}

int uthread_get_tid()
{
	auto mutex = ctx_switch_mutex();
	return uthread_mgr::get_instance().running_thread->get_id();
}

int uthread_get_total_quantums()
{
	auto mutex = ctx_switch_mutex();
	return uthread_mgr::get_instance().elapsed_quantums;
}

int uthread_get_quantums(int tid)
{
	auto mutex = ctx_switch_mutex();
	const auto thread = uthread_mgr::get_instance().threads.find(tid);
	if (thread == uthread_mgr::get_instance().threads.end())
	{
		print_library_error("thread id not found");
		return -1;
	}

	return thread->second->get_elapsed_quantums();
}
