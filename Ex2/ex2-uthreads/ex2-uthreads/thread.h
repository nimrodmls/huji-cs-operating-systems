#ifndef THREAD_H
#define THREAD_H

#include <setjmp.h>

#include "uthreads.h"

enum pause_state : int
{
	SUSPENDED,
	RESUMED
};

enum thread_state : int
{
	READY,
	RUNNING,
	BLOCKED
};

/* Represents a thread in the User-Threads Library */
class thread
{
public:
	// Constructor for regular user thread
	thread(thread_id id, thread_entry_point ep);
	// Constructor for thread forked from the current thread,
	// unlike starting from a whole new EP (primarily for the main thread)
	thread(thread_id id);

	// Start/Continue running the thread
	void run();
	// Pause the thread. Returns pause_state::SUSPENDED if the thread was paused successfully,
	// returns pause_state::RESUMED if the thread was resumed, thus continuing its execution
	// from the point it was paused
	int suspend(bool is_blocked);

	void increment_elapsed_quantums() { elapsed_quantums++; }
	int get_elapsed_quantums() const { return elapsed_quantums; }

	thread_state get_state() const { return state; }
	void set_state(thread_state new_state) { state = new_state;  }

	int get_sleep_time() const { return sleep_time;  }
	void set_sleep_time(int time) { sleep_time = time; }

	sigjmp_buf& get_env_block() { return env_blk; }
	sigjmp_buf env_blk;

	unsigned int get_id() const { return id; }

private:

	thread_id id;
	// The environment block of the thread
	char stack[STACK_SIZE];
	thread_state state;
	int sleep_time;
	int elapsed_quantums;
};

#endif // THREAD_H