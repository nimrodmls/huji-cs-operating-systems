#ifndef MUTEX_H
#define MUTEX_H

#include <pthread.h>

class Mutex
{
public:
	Mutex();
	Mutex(const Mutex&) = delete;
	Mutex& operator=(const Mutex&) = delete;
	~Mutex() = default;

	void lock();
	void unlock();

private:
	pthread_mutex_t m_mutex;
};

#endif // MUTEX_H