#ifndef SEMAPHORE_H
#define SEMAPHORE_H

#include <cstdint>
#include <semaphore.h>

/* RAII wrapper for a Thread-only semaphore
 * It's called CSemaphore just because the header name may conflict
 */
class CSemaphore
{
public:
	CSemaphore(uint32_t initial);
	CSemaphore(const CSemaphore&) = delete;
	CSemaphore& operator=(const CSemaphore&) = delete;
	~CSemaphore();

	void wait();
	void post();

private:

	static sem_t create_semaphore(uint32_t initial);

	sem_t m_semaphore;
};

#endif // SEMAPHORE_H