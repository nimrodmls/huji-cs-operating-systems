#include <pthread.h>

// Entrypoint for a thread - Receives a ptr to the arguments
// Allocation and access to the arguments is at the responsibility of the caller
using ThreadEntrypoint = void* (*)(void*);

/* A class representing a thread
 * Note - On failure of system calls, the program will exit
 */
class Thread
{
public:
	Thread(const ThreadEntrypoint& entrypoint);
	Thread(const Thread&) = delete;
	Thread& operator=(const Thread&) = delete;
	~Thread() = default;

	// Starting the thread with the given arguments
	void run(void* args);

	// Waiting on the thread
	void join() const;

private:
	pthread_t m_thread;
	ThreadEntrypoint m_entrypoint;
};
