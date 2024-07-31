#include <algorithm>
#include <iostream>
#include <memory>
#include <queue>
#include <functional>
#include <signal.h>
#include <sys/time.h>
#include <numeric>

#include "thread.h"
#include "uthreads.h"

/*
 * NOTE: The code uses exit(1) as a way to terminate the program in case of a system error,
 *		 as per the exercise requirements. This is not a good practice in a real-world application,
 *		 and the error should be propagated to the caller instead.
 */

/* Constants */
constexpr uint32_t main_thread_id = 0;
constexpr uint32_t usec_threshold = 1000000;
using thread_id_queue = std::priority_queue<thread_id, std::vector<thread_id>, std::greater<thread_id>>;

/* Enum for the return status of the library functions */
enum return_status : int
{
	STATUS_FAILURE = -1,
	STATUS_SUCCESS = 0
};

/* Enum for the jump suspend state of the thread */
enum suspend_state : int
{
	SUSPENDED,
	RESUMED
};

/* Helper functions for printing errors */
static void print_library_error(const std::string& msg)
{
	std::cerr << "thread library error: " << msg << std::endl;
}

static void print_system_error(const std::string& msg)
{
	std::cerr << "system error: " << msg << std::endl;
}

/* RAII class for disabling and enabling context switching */
class ctx_switch_lock
{
public:
	ctx_switch_lock() { disable_ctx_switch(); }
	~ctx_switch_lock() { enable_ctx_switch(); }

	ctx_switch_lock(const ctx_switch_lock&) = delete;
	ctx_switch_lock operator=(const ctx_switch_lock&) = delete;

private:
	static void disable_ctx_switch()
	{
		sigset_t sigset;
		sigemptyset(&sigset);
		sigaddset(&sigset, SIGVTALRM);
		if (-1 == sigprocmask(SIG_BLOCK, &sigset, nullptr))
		{
			print_system_error("lock - failed to disable context switching");
			exit(1);
		}
	}

	static void enable_ctx_switch()
	{
		sigset_t sigset;
		sigemptyset(&sigset);
		sigaddset(&sigset, SIGVTALRM);
		if (-1 == sigprocmask(SIG_UNBLOCK, &sigset, nullptr))
		{
			print_system_error("lock - failed to reenable context switching");
			exit(1);
		}
	}
};

/* Manager for the user threads */
class uthread_mgr
{

public:
	uthread_mgr() :
		quantum_usecs_interval(0),
		elapsed_quantums(0),
		running_thread(0),
		threads()
	{}

	int quantum_usecs_interval;
	int elapsed_quantums;
	thread_id_queue available_ids;
	// Storing all the threads that are ready to run, as a queue,
	// the threads will be run in a FIFO order
	std::deque<thread_id> ready_threads;
	// The current running thread
	thread_id running_thread;
	// Storing all the active threads (on all states)
	thread* threads[MAX_THREAD_NUM];
	// Storing all the threads marked for deletion
	std::vector<thread_id> to_delete;
};

static uthread_mgr g_mgr;

/* Resets the timer to the quantum interval set in the manager */
static void reset_timer()
{
	// Setting up the timer
	struct itimerval timer = { 0 };
	timer.it_value.tv_sec = g_mgr.quantum_usecs_interval / usec_threshold;
	timer.it_value.tv_usec = g_mgr.quantum_usecs_interval % usec_threshold;
	timer.it_interval.tv_sec = g_mgr.quantum_usecs_interval / usec_threshold;
	timer.it_interval.tv_usec = g_mgr.quantum_usecs_interval % usec_threshold;

	if (-1 == setitimer(ITIMER_VIRTUAL, &timer, nullptr))
	{
		print_system_error("init - timer setup failed");
		exit(1);
	}
}

/* Deletes a thread from the manager */
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
			// If the thread is being blocked, we reset the timer to allow
			// full quantum for the next thread
			reset_timer();
			g_mgr.threads[g_mgr.running_thread]->state = BLOCKED;
		}

		if (terminate_running)
		{
			g_mgr.to_delete.push_back(g_mgr.running_thread);
		}

		g_mgr.running_thread = g_mgr.ready_threads.front(); // Getting the next thread to run
		g_mgr.ready_threads.pop_front(); // Removing the thread from the queue

		g_mgr.elapsed_quantums++;
		g_mgr.threads[g_mgr.running_thread]->elapsed_quantums++;
		g_mgr.threads[g_mgr.running_thread]->state = RUNNING;

		siglongjmp(g_mgr.threads[g_mgr.running_thread]->env_blk, RESUMED);
	}
}

static void handle_sleeper_threads()
{
	for (int it = 0; it < MAX_THREAD_NUM; it++)
	{
		if (nullptr == g_mgr.threads[it])
		{
			continue;
		}

		// Finding all the sleeping threads and decrementing their sleep time
		if (0 != g_mgr.threads[it]->sleep_time)
		{
			g_mgr.threads[it]->sleep_time--;
			// If the sleep time has passed, we should wake up the thread
			// that is if and only if the thread is not blocked
			// (if the thread is blocked, it will be woken up by uthread_resume)
			if ((0 == g_mgr.threads[it]->sleep_time) && 
				(!g_mgr.threads[it]->is_blocked))
			{
				g_mgr.threads[it]->state = READY;
				g_mgr.ready_threads.push_back(it);
			}
		}
	}
}

static void sigvtalrm_handler(int sig_num)
{
	// Signal number is not used - We expect this function to be called only by the timer

	// Readying up the sleeper threads (if any)
	handle_sleeper_threads();

	// Switching to the next thread
	switch_threads(false, false);
}

int uthread_init(int quantum_usecs)
{
	// Non-positive quantum_usecs is considered an error
	if (quantum_usecs <= 0)
	{
		print_library_error("init - invalid quantum interval value");
		return STATUS_FAILURE;
	}

	g_mgr.quantum_usecs_interval = quantum_usecs;
	g_mgr.elapsed_quantums = 1;

	// Setting up the available thread IDs
	std::vector<thread_id> available(MAX_THREAD_NUM-1);
	std::iota(available.begin(), available.end(), 1);
	g_mgr.available_ids = thread_id_queue(std::greater<thread_id>(), available);

	// Setting up the main thread
	thread* main_thread = nullptr;
	try
	{
		main_thread = new thread(main_thread_id);
	}
	catch (const std::bad_alloc&)
	{
		print_system_error("init - thread allocation failed");
		exit(1);
	}

	g_mgr.threads[main_thread_id] = main_thread;
	g_mgr.running_thread = main_thread_id;

	// Setting up the sigaction associated with the timer
	struct sigaction new_action = { 0 };
	new_action.sa_handler = sigvtalrm_handler;

	int sigset_retval = 0;
	sigset_retval += sigemptyset(&new_action.sa_mask);
	sigset_retval += sigaddset(&new_action.sa_mask, SIGVTALRM);
	sigset_retval += sigaction(SIGVTALRM, &new_action, nullptr);
	if (0 != sigset_retval)
	{
		print_system_error("init - failed to setup signal handling");
		exit(1);
	}

	// Setting up the timer
	reset_timer();

	return STATUS_SUCCESS;
}

int uthread_spawn(thread_entry_point entry_point)
{
	ctx_switch_lock mutex{};

	// First, deleting all the threads which are done
	for (const thread_id id : g_mgr.to_delete)
	{
		delete_thread(id);
	}
	g_mgr.to_delete.clear();

	if (g_mgr.available_ids.empty())
	{
		print_library_error("spawn - maximum number of threads reached");
		return STATUS_FAILURE;
	}

	const thread_id tid = g_mgr.available_ids.top();
	g_mgr.available_ids.pop();

	// Creating the new thread
	thread* new_thread = nullptr;
	try
	{
		new_thread = new thread(tid, entry_point);
	}
	catch (const std::bad_alloc&)
	{
		print_system_error("spawn - thread allocation failed");
		exit(1);
	}

	// Mapping the new thread and marking it ready
	g_mgr.threads[tid] = new_thread;
	g_mgr.ready_threads.push_back(tid);

	return tid;
}

int uthread_terminate(int tid)
{
	ctx_switch_lock mutex{};

	// If the requested thread to terminate is the main thread, we should exit the program
	// as specified in the exercise. All the user threads will be deleted.
	if (main_thread_id == tid)
	{
		for (const thread* t : g_mgr.threads)
		{
			delete t;
		}
		exit(0);
	}

	auto thread = g_mgr.threads[tid];
	if (nullptr == thread)
	{
		print_library_error("terminate - thread id not found");
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
	ctx_switch_lock mutex{};

	if (main_thread_id == tid)
	{
		print_library_error("block - cannot block the main thread");
		return STATUS_FAILURE;
	}

	if (tid == g_mgr.running_thread)
	{
		// The running thread is blocking itself, we should switch to the next thread
		g_mgr.threads[tid]->is_blocked = true;
		switch_threads(true, false);
	}
	else
	{
		// Looking up the thread
		const auto thread = g_mgr.threads[tid];
		if (nullptr == thread)
		{
			print_library_error("block - thread id not found");
			return STATUS_FAILURE;
		}

		// If the thread was found, looking for it in the ready queue and erasing it
		const auto find_result =
			std::find(g_mgr.ready_threads.begin(), g_mgr.ready_threads.end(), tid);
		if (find_result != g_mgr.ready_threads.end())
		{
			g_mgr.ready_threads.erase(find_result);
		}

		g_mgr.threads[tid]->state = BLOCKED;
		g_mgr.threads[tid]->is_blocked = true;
	}

	return STATUS_SUCCESS;
}

int uthread_resume(int tid)
{
	ctx_switch_lock mutex{};

	// Looking up the thread
	const auto thread = g_mgr.threads[tid];
	if (nullptr == thread)
	{
		print_library_error("resume - thread id not found");
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
	ctx_switch_lock mutex{};
	if (main_thread_id == g_mgr.running_thread)
	{
		print_library_error("sleep - cannot sleep the main thread");
		return STATUS_FAILURE;
	}

	// Putting the thread to sleep
	g_mgr.threads[g_mgr.running_thread]->sleep_time = num_quantums;
	// Switching to the next thread
	switch_threads(true, false);

	return STATUS_SUCCESS;
}

int uthread_get_tid()
{
	ctx_switch_lock mutex{};
	return g_mgr.running_thread;
}

int uthread_get_total_quantums()
{
	ctx_switch_lock mutex{};
	return g_mgr.elapsed_quantums;
}

int uthread_get_quantums(int tid)
{
	ctx_switch_lock mutex{};

	const auto thread = g_mgr.threads[tid];
	if (nullptr == thread)
	{
		print_library_error("get_quantums - thread id not found");
		return STATUS_FAILURE;
	}

	return thread->elapsed_quantums;
}
