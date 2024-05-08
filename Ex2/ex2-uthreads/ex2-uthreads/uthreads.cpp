#include <iostream>
#include <memory>
#include <queue>
#include <list>
#include <functional>
#include <signal.h>
#include <sys/time.h>
#include <numeric>

#include "thread.h"
#include "uthreads.h"

#define MAIN_THREAD_ID 0

enum return_status : int
{
	STATUS_SUCCESS,
	STATUS_FAILURE
};

enum suspend_state : int
{
	SUSPENDED,
	RESUMED
};

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
struct uthread_mgr
{
	int quantum_usecs_interval;
	int elapsed_quantums;
	std::priority_queue<thread_id, std::vector<thread_id>, std::greater<>> available_ids;
	std::list<thread_id> sleeper_threads;
	// Storing all the threads that are ready to run, as a queue,
	// the threads will be run in a FIFO order
	std::deque<thread_id> ready_threads;
	// The current running thread
	thread_id running_thread;
	// Storing all the active threads (on all states)
	thread* threads[MAX_THREAD_NUM];
	// Storing all the threads marked for deletion
	std::list<thread_id> to_delete;
};

static uthread_mgr g_mgr;

static void print_library_error(const std::string& msg)
{
	std::cerr << "thread library error: " << msg << std::endl;
}

static void print_system_error(const std::string& msg)
{
	std::cerr << "system error: " << msg << std::endl;
}

static void delete_thread(thread_id tid)
{
	delete g_mgr.threads[tid];
	g_mgr.threads[tid] = nullptr;
	g_mgr.available_ids.push(tid);
}

static void switch_threads(bool is_blocked, bool terminate_running)
{
	const int paused = sigsetjmp(g_mgr.threads[g_mgr.running_thread]->env_blk, 1);
	// The thread has been paused, we should switch to the next thread
	// in the ready queue
	if (suspend_state::SUSPENDED == paused)
	{
		// Pushing the paused thread to the end of the queue, only if the switching
		// was not triggered by a block
		if (!is_blocked)
		{
			g_mgr.threads[g_mgr.running_thread]->state = READY;
			g_mgr.ready_threads.push_back(g_mgr.running_thread);
		}
		else
		{
			g_mgr.threads[g_mgr.running_thread]->state = BLOCKED;
		}

		// If the thread is terminating, we should remove it from the threads map
		// WE SHOULD NOT REACH HERE DURING SIGNAL HANDLING - SUPER DANGEROUS
		if (terminate_running)
		{
			g_mgr.to_delete.emplace_back(g_mgr.running_thread);
			//delete_thread(g_mgr.running_thread);
		}

		g_mgr.running_thread = g_mgr.ready_threads.front(); // Getting the next thread to run
		g_mgr.ready_threads.pop_front(); // Removing the thread from the queue

		siglongjmp(g_mgr.threads[g_mgr.running_thread]->env_blk, RESUMED);
	}
	else
	{
		const auto running = g_mgr.threads[g_mgr.running_thread];
		running->state = RUNNING;
		running->elapsed_quantums++;

		for (const auto tid : g_mgr.to_delete)
		{
			delete_thread(tid);
		}
		g_mgr.to_delete.clear();
	}
}

static void handle_sleeper_threads()
{
	std::list<thread_id> to_remove;
	for (auto iter = g_mgr.sleeper_threads.begin();
		iter != g_mgr.sleeper_threads.end(); ++iter)
	{
		g_mgr.threads[*iter]->sleep_time--;
		if (0 == g_mgr.threads[*iter]->sleep_time)
		{
			to_remove.emplace_back(*iter);
			g_mgr.threads[*iter]->state = READY;
			g_mgr.ready_threads.push_back(*iter);
		}
	}

	g_mgr.sleeper_threads.remove_if([&to_remove](thread_id tid)
	{
		return std::find(
			to_remove.begin(), to_remove.end(), tid) != to_remove.end();
	});
}

static void sigvtalrm_handler(int sig_num)
{
	handle_sleeper_threads();
	switch_threads(false, false);
	g_mgr.elapsed_quantums++;
}

int uthread_init(int quantum_usecs)
{
	// Non-positive quantum_usecs is considered an error
	if (quantum_usecs <= 0)
	{
		print_library_error("invalid quantum interval value");
		return STATUS_FAILURE;
	}

	g_mgr.quantum_usecs_interval = quantum_usecs;
	g_mgr.elapsed_quantums = 1;

	// Setting up the available thread IDs
	std::vector<thread_id> available(MAX_THREAD_NUM-1);
	std::iota(available.begin(), available.end(), 1);
	g_mgr.available_ids = std::priority_queue(std::greater(), available);

	// Setting up the main thread
	thread* main_thread = nullptr;
	try
	{
		main_thread = new thread(MAIN_THREAD_ID);
	}
	catch (const std::bad_alloc&)
	{
		print_system_error("thread allocation failed");
		return STATUS_FAILURE;
	}
	g_mgr.threads[MAIN_THREAD_ID] = main_thread;
	g_mgr.running_thread = MAIN_THREAD_ID;

	// Setting up the sigaction associated with the timer
	struct sigaction new_action = { 0 };
	new_action.sa_handler = sigvtalrm_handler;
	sigemptyset(&new_action.sa_mask);
	sigaddset(&new_action.sa_mask, SIGVTALRM);
	sigaction(SIGVTALRM, &new_action, NULL);

	// Setting up the timer
	struct itimerval timer = { 0 };
	timer.it_value.tv_usec = g_mgr.quantum_usecs_interval;
	timer.it_interval.tv_usec = g_mgr.quantum_usecs_interval;

	if (-1 == setitimer(ITIMER_VIRTUAL, &timer, NULL))
	{
		print_system_error("timer setup failed");
		return STATUS_FAILURE;
	}

	return STATUS_SUCCESS;
}

int uthread_spawn(thread_entry_point entrypoint)
{
	auto mutex = ctx_switch_mutex();
	if (g_mgr.available_ids.empty())
	{
		print_library_error("maximum number of threads reached");
		return STATUS_FAILURE;
	}

	const thread_id tid = g_mgr.available_ids.top();
	g_mgr.available_ids.pop();

	// Creating the new thread
	thread* new_thread = nullptr;
	try
	{
		new_thread = new thread(tid, entrypoint);
		
	}
	catch (const std::bad_alloc&)
	{
		print_system_error("thread allocation failed");
		return STATUS_FAILURE;
	}
	// Mapping the new thread and marking it ready
	g_mgr.threads[tid] = new_thread;
	g_mgr.ready_threads.push_back(tid);

	return tid;
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

	auto thread = g_mgr.threads[tid];
	if (nullptr == thread)
	{
		print_library_error("thread id not found");
		return STATUS_FAILURE;
	}

	// If the thread is running, we should switch to the next thread and erase this one
	if (RUNNING == thread->state)
	{
		switch_threads(true, true);
	}

	// If the thread is ready, we should erase it from the ready queue
	if (READY == thread->state)
	{
		const auto find_result =
			std::find(g_mgr.ready_threads.begin(),
					g_mgr.ready_threads.end(),
					  thread->id);
		if (find_result != g_mgr.ready_threads.end())
		{
			g_mgr.ready_threads.erase(find_result);
		}
	}

	// We can safely do this here since we will reach here only if the thread is READY or BLOCKED
	// (because on RUNNING, we will switch to another thread and never return here)
	delete_thread(tid);

	return STATUS_SUCCESS;
}

int uthread_block(int tid)
{
	auto mutex = ctx_switch_mutex();

	if (MAIN_THREAD_ID == tid)
	{
		print_library_error("cannot block the main thread");
		return STATUS_FAILURE;
	}

	if (tid == g_mgr.running_thread)
	{
		// The running thread is blocking itself, we should switch to the next thread
		switch_threads(true, false);
	}
	else
	{
		// Looking up the thread
		const auto thread = g_mgr.threads[tid];
		if (nullptr == thread)
		{
			print_library_error("thread id not found");
			return STATUS_FAILURE;
		}

		// If the thread was found, looking for it in the ready queue and erasing it
		const auto find_result =
			std::find(g_mgr.ready_threads.begin(), g_mgr.ready_threads.end(), tid);
		if (find_result != g_mgr.ready_threads.end())
		{
			g_mgr.ready_threads.erase(find_result);
			g_mgr.threads[tid]->state = BLOCKED;
		}
	}

	return STATUS_SUCCESS;
}

int uthread_resume(int tid)
{
	auto mutex = ctx_switch_mutex();

	// Looking up the thread
	const auto thread = g_mgr.threads[tid];
	if (nullptr == thread)
	{
		print_library_error("thread id not found");
		return STATUS_FAILURE;
	}

	// We resume the thread only if it is blocked and its sleep time has passed
	// Therefore, resuming READY and RUNNING threads, or BLOCKED threads with
	// remaining sleep time is not an error, and simply ignored.
	if ((BLOCKED == thread->state) &&
		(0 == thread->sleep_time))
	{
		g_mgr.ready_threads.push_back(thread->id);
		thread->state = READY;
	}

	return STATUS_SUCCESS;
}

int uthread_sleep(int num_quantums)
{
	auto mutex = ctx_switch_mutex();
	if (MAIN_THREAD_ID == g_mgr.running_thread)
	{
		print_library_error("cannot sleep the main thread");
		return STATUS_FAILURE;
	}

	// Putting the thread to sleep
	g_mgr.sleeper_threads.emplace_back(g_mgr.running_thread);
	g_mgr.threads[g_mgr.running_thread]->sleep_time = num_quantums;
	// Switching to the next thread
	switch_threads(true, false);

	return STATUS_SUCCESS;
}

int uthread_get_tid()
{
	auto mutex = ctx_switch_mutex();
	return g_mgr.running_thread;
}

int uthread_get_total_quantums()
{
	auto mutex = ctx_switch_mutex();
	return g_mgr.elapsed_quantums;
}

int uthread_get_quantums(int tid)
{
	auto mutex = ctx_switch_mutex();

	const auto thread = g_mgr.threads[tid];
	if (nullptr == thread)
	{
		print_library_error("thread id not found");
		return STATUS_FAILURE;
	}

	return thread->elapsed_quantums;
}
