#ifndef BARRIER_H
#define BARRIER_H

class Barrier
{
public:
	Barrier(uint32_t numThreads);
	Barrier(const Barrier&) = delete;
	Barrier& operator=(const Barrier&) = delete;
	~Barrier();

	void barrier();

private:
	pthread_mutex_t mutex;
	pthread_cond_t cv;
	uint32_t count;
	uint32_t numThreads;
};

#endif //BARRIER_H
