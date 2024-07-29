#include <pthread.h>

#include "Common.h"
#include "Barrier.h"

Barrier::Barrier(uint32_t numThreads) :
	mutex(PTHREAD_MUTEX_INITIALIZER),
	cv(PTHREAD_COND_INITIALIZER),
	count(0),
	numThreads(numThreads)
{ }

Barrier::~Barrier()
{
	try
	{
		if (0 != pthread_mutex_destroy(&mutex)) 
		{
			Common::emit_system_error("pthread_mutex_destroy failed");
		}

		if (0 != pthread_cond_destroy(&cv))
		{
			Common::emit_system_error("pthread_cond_destroy failed");
		}
	}
	catch (...)
	{}
}


void Barrier::barrier()
{
	if (0 != pthread_mutex_lock(&mutex))
	{
		Common::emit_system_error("pthread_mutex_lock failed");
	}

	if (++count < numThreads) 
	{
		if (0 != pthread_cond_wait(&cv, &mutex))
		{
			Common::emit_system_error("pthread_cond_wait failed");
		}
	}
	else 
	{
		count = 0;
		if (0 != pthread_cond_broadcast(&cv)) 
		{
			Common::emit_system_error("pthread_cond_broadcast failed");
		}
	}
	if (0 != pthread_mutex_unlock(&mutex)) 
	{
		Common::emit_system_error("pthread_mutex_unlock failed");
	}
}
