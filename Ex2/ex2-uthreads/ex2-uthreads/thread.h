#ifndef THREAD_H
#define THREAD_H

#include <setjmp.h>

#include "uthreads.h"

enum pause_state : int
{
	PAUSED,
	RESUMED
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
	// Pause the thread. Returns pause_state::PAUSED if the thread was paused successfully,
	// returns pause_state::RESUMED if the thread was resumed, thus continuing its execution
	// from the point it was paused
	int pause();

	unsigned int get_id() const { return id; }

private:

	// The thread's ID (TID)
	unsigned int id;
	// The environment block of the thread
	sigjmp_buf env_blk;
	char stack[STACK_SIZE];
};

#endif // THREAD_H