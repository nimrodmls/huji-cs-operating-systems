#ifndef SEMAPHORE_H
#define SEMAPHORE_H

#include <cstdint>
#include <semaphore.h>

/* RAII wrapper for a Thread-only semaphore */
class Semaphore
{
public:
	Semaphore(uint32_t initial);
	Semaphore(const Semaphore&) = delete;
	Semaphore& operator=(const Semaphore&) = delete;
	~Semaphore();

	void wait();
	void post();

private:

	static sem_t create_semaphore(uint32_t initial);

	sem_t m_semaphore;
};

#endif // SEMAPHORE_H