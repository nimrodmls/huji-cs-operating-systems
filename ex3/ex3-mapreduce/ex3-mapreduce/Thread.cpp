#include <pthread.h>

#include "Thread.h"
#include "Common.h"

Thread::Thread(const ThreadEntrypoint& entrypoint, void* args) :
	m_joined(false),
	m_thread(),
	m_ep_args(args),
	m_entrypoint(entrypoint)
{}

void Thread::run()
{
	const int status = pthread_create(&m_thread, nullptr, m_entrypoint, m_ep_args);
	if (0 != status)
	{
		Common::emit_system_error("pthread_create failed");
	}
}

void Thread::join()
{
	// Note that we allow a thread to be joined ONCE.
	// Otherwise, undefined behavior will occur.
	if (!m_joined)
	{
		const int status = pthread_join(m_thread, nullptr);
		// ESRCH is returned if the thread has already been joined, which is not an error
		if ((0 != status) && (ESRCH != status))
		{
			Common::emit_system_error("pthread_join failed");
		}
		m_joined = true;
	}
}
