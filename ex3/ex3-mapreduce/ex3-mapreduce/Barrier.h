#ifndef BARRIER_H
#define BARRIER_H

class Barrier
{
public:
	Barrier(int numThreads);
	~Barrier();
	void barrier();

private:
	pthread_mutex_t mutex;
	pthread_cond_t cv;
	int count;
	int numThreads;
};

#endif //BARRIER_H
