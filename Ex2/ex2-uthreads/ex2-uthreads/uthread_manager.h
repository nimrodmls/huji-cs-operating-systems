#ifndef	UTHREAD_MANAGER_H
#define UTHREAD_MANAGER_H

#include <map>
#include <memory>
#include <queue>
#include <set>

#include "thread.h"
#include "uthreads.h"

// Generic uthread library exception
class uthread_exception : public std::exception
{
public:
	uthread_exception(const std::string& message)
		: message("thread library error: " + message)
	{}

	const char* what() const noexcept override { return message.c_str(); }

private:
	std::string message;
};

class system_exception : public std::exception
{
public:
	system_exception(const std::string& message)
		: message("system error: " + message)
	{}

	const char* what() const noexcept override { return message.c_str(); }

private:
	std::string message;
};

class uthread_manager
{
public:
	explicit uthread_manager(int quantum_usecs);
	~uthread_manager() = default;

	uthread_manager(const uthread_manager&) = delete;
	uthread_manager operator=(const uthread_manager&) = delete;

	// Thread-manipulating functions
	void init_main_thread();
	void spawn(thread_entry_point entry_point);
	void terminate(thread_id tid);
	void block(thread_id tid);
	void resume(thread_id tid);
	// Switches the RUNNING thread to sleep mode
	void sleep(int sleep_quantums);

	// Getters
	thread_id get_tid() const;
	int get_total_elapsed_quantums() const { return elapsed_quantums; }
	int get_elapsed_quantums(thread_id tid) const;

	static constexpr int MAIN_THREAD_ID = 0;

private:

	// Switching from the currently running thread to the next ready thread
	void switch_threads(bool is_blocked, bool terminate_running);

	// The quantum timer signal handler. It is responsible for
	// switching between threads, and waking up sleeping threads.
	static void sigvtalrm_handler(int sig_num);

	// The length of each quantum in microseconds
	int quantum_usecs_interval;
	int elapsed_quantums;
	// Storing all the threads that are ready to run, as a queue,
	// the threads will be run in a FIFO order
	std::deque<thread_id> ready_threads;
	// The current running thread
	std::shared_ptr<thread> running_thread;
	// Storing all the active threads (on all states)
	std::map<thread_id, std::shared_ptr<thread>> threads;
	sigjmp_buf env[3];
};

// Singleton-type class for storing the singular instance of uthread_manager.
// More can be created manually, although the implications are not defined.
class global_uthread_manager
{
public:
	static void init(int quantum_usecs);
	static uthread_manager& get_instance();

private:
	static uthread_manager& get_instance(int quantum_usecs);
};

#endif // UTHREAD_MANAGER_H