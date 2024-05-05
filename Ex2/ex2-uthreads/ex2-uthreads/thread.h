#ifndef THREAD_H
#define THREAD_H

#include <setjmp.h>

#include "uthreads.h"

/* Represents a thread in the User-Threads Library */
class thread
{
public:
	thread(unsigned int id, thread_entry_point ep);

	void start();

	unsigned int get_id() const { return id; }
	
private:
	static const int JMP_RET_VAL = 1;

	// The thread's ID (TID)
	unsigned int id;
	// The environment block of the thread
	sigjmp_buf env_blk;
	char stack[STACK_SIZE];
};

#endif // THREAD_H