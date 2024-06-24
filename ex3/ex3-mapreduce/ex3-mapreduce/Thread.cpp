#include <pthread.h>

#include "Thread.h"
#include "Common.h"

Thread::Thread(const ThreadEntrypoint& entrypoint, void* args) :
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

void Thread::join() const
{
	const int status = pthread_join(m_thread, nullptr);
	if (0 != status)
	{
		Common::emit_system_error("pthread_join failed");
	}
}
