#include "Common.h"
#include "Mutex.h"

Mutex::Mutex() :
	m_mutex(PTHREAD_MUTEX_INITIALIZER)
{}

void Mutex::lock()
{
	if (0 != pthread_mutex_lock(&m_mutex))
	{
		Common::emit_system_error("pthread_mutex_lock failed");
	}
}

void Mutex::unlock()
{
	if (0 != pthread_mutex_unlock(&m_mutex))
	{
		Common::emit_system_error("pthread_mutex_unlock failed");
	}
}

AutoMutexLock::AutoMutexLock(MutexPtr& mutex) :
	m_mutex(mutex)
{
	m_mutex->lock();
}

AutoMutexLock::~AutoMutexLock()
{
	try
	{
		m_mutex->unlock();
	}
	catch (...)
	{}
}