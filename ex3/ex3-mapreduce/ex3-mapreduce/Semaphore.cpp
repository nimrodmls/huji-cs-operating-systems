#include "Common.h"
#include "Semaphore.h"

Semaphore::Semaphore(uint32_t initial) :
	m_semaphore(create_semaphore(initial))
{}

Semaphore::~Semaphore()
{
	if (0 != sem_destroy(&m_semaphore))
	{
		Common::emit_system_error("sem_destroy failed");
	}
}

void Semaphore::wait()
{
	if (0 != sem_wait(&m_semaphore))
	{
		Common::emit_system_error("sem_wait failed");
	}
}

void Semaphore::post()
{
	if (0 != sem_post(&m_semaphore))
	{
		Common::emit_system_error("sem_post failed");
	}
}

sem_t Semaphore::create_semaphore(uint32_t initial)
{
	sem_t sem;
	if (0 != sem_init(&sem, 0, initial))
	{
		Common::emit_system_error("sem_init failed");
	}
	return sem;
}
