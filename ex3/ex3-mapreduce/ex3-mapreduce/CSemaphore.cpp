#include "Common.h"
#include "CSemaphore.h"

CSemaphore::CSemaphore(uint32_t initial) :
	m_semaphore(create_semaphore(initial))
{}

CSemaphore::~CSemaphore()
{
	if (0 != sem_destroy(&m_semaphore))
	{
		Common::emit_system_error("sem_destroy failed");
	}
}

void CSemaphore::wait()
{
	if (0 != sem_wait(&m_semaphore))
	{
		Common::emit_system_error("sem_wait failed");
	}
}

void CSemaphore::post()
{
	if (0 != sem_post(&m_semaphore))
	{
		Common::emit_system_error("sem_post failed");
	}
}

sem_t CSemaphore::create_semaphore(uint32_t initial)
{
	sem_t sem;
	if (0 != sem_init(&sem, 0, initial))
	{
		Common::emit_system_error("sem_init failed");
	}
	return sem;
}
