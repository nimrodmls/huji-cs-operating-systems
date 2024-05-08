#ifndef THREAD_H
#define THREAD_H

#include <setjmp.h>

#include "uthreads.h"

using thread_id = int;

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
	~thread();

	const thread_id id;
	// The environment block of the thread
	sigjmp_buf env_blk;
	thread_state state;
	int sleep_time;
	int elapsed_quantums;

private:
	char* stack;
};

#endif // THREAD_H