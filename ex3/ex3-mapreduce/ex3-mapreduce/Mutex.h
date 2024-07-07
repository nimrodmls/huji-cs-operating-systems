#ifndef MUTEX_H
#define MUTEX_H

#include <memory>
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

using MutexPtr = std::shared_ptr<Mutex>;

class AutoMutexLock
{
public:
	AutoMutexLock(MutexPtr& mutex);
	AutoMutexLock(const AutoMutexLock&) = delete;
	AutoMutexLock& operator=(const AutoMutexLock&) = delete;
	~AutoMutexLock();

private:
	const MutexPtr m_mutex;
};

#endif // MUTEX_H