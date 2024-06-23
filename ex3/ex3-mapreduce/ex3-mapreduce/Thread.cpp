#include "Thread.h"
#include "Common.h"

Thread::Thread(const ThreadEntrypoint& entrypoint) :
	m_thread(),
	m_entrypoint(entrypoint)
{}

void Thread::run(void* args)
{
	const int status = pthread_create(&m_thread, nullptr, m_entrypoint, args);
	if (0 != status)
	{
		emit_system_error("pthread_create failed");
	}
}

void Thread::join() const
{
	const int status = pthread_join(m_thread, nullptr);
	if (0 != status)
	{
		emit_system_error("pthread_join failed");
	}
}
