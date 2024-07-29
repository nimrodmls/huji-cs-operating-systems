#ifndef JOB_CONTEXT_H
#define JOB_CONTEXT_H

#include <atomic>
#include <queue>

#include "MapReduceFramework.h"
#include "Thread.h"
#include "Barrier.h"
#include "Mutex.h"
#include "CSemaphore.h"

class Job;

/*
 * A Context for the worker thread
 * The job will create a separate one for each worker
 */
class WorkerContext
{
public:
	WorkerContext(Job* job_context) :
		jobContext(job_context),
		intermediateVec()
	{}

	// Reference to the owning job context
	Job* jobContext;
	// The worker's intermediate vector
	IntermediateVec intermediateVec;
};

using WorkerContextUPtr = std::unique_ptr<WorkerContext>;

/*
 * The Job is the workhorse of the MapReduce framework
 * Within this class resides all the logic from end-to-end
 * (apart from the emit parts, which are in MapReduceFramework.cpp)
 * The Job is responsible for creating the worker threads, managing the stages,
 * and calling the client-side.
 * The majority of the work is done within job_worker_thread, which in
 * turn works on each stage of the job and synchronizes with other workers.
 */
class Job
{
public:
	Job(
		const InputVec& inputVec, 
		OutputVec& outputVec, 
		const MapReduceClient& client,
		uint32_t worker_count);
	Job(const Job&) = delete;
	Job& operator=(const Job&) = delete;
	// The dtor is not waiting for the worker threads to finish
	// it is in the responsibility of the caller to wait for the job to finish,
	// otherwise the threads will be forcefully terminated
	~Job() = default;

	// Starting the job, by starting all the worker threads
	void start_job();

	// Waiting on the job to finish
	void wait();

	// Retreiving the current state of the job
	void get_state(JobState* state) const;

	void add_output(K3* key, V3* value);

private:
	// Adding a worker thread
	void add_worker();

	// Stage status utilities
	stage_t get_stage() const;
	uint32_t get_stage_total() const;
	void set_stage(stage_t new_stage, uint32_t total);
	uint32_t inc_stage_processed(uint32_t val);

	/* Atomically assigning the shuffle job
	 * Returns true if the shuffle job has been assigned to the caller,
	 * false otherwise */
	bool assign_shuffle_job();

	/* -- Worker Utility function --
	 * Worker's map/reduce stage handler */
	static void worker_handle_current_stage(
		WorkerContext* worker_ctx);

	/**
	 * -- Worker Utility function --
	 * The shuffle stage is executed by one of the worker threads
	 * The shuffle stage is responsible for grouping the intermediates by key
	 * Note: This function is NOT thread-safe, as it is only called by one worker */
	static void worker_shuffle_stage(Job* job_context);

	/**
	 * Entrypoint for a job worker thread
	 * The worker thread will execute map-sort-reduce operations
	 * for the job. Multiple worker threads can run concurrently
	 *
	 * @param context - The worker context
	 * @return - Always NULL
	 */
	static void* job_worker_thread(void* context);

	InputVec m_inputVec;
	OutputVec& m_outputVec;
	const MapReduceClient& m_client;
	Barrier m_shuffle_barrier;
	CSemaphore m_shuffle_semaphore;
	// Mutex for synchronizing access to the output vector when reducing (add_output)
	MutexPtr m_output_mutex;
	// Mutex for synchronizing access to the shuffle queue when reducing (worker)
	MutexPtr m_reduce_mutex;
	/* The stage counter is a 64-bit bitfield, with the following structure:
	 * 31-bit processed entries counter, 31-bit total entries counter, 2-bit stage ID */
	std::atomic<uint64_t> m_stage_status;
	// Boolean flag to indicate whether the shuffle job has been assigned to one of the workers
	std::atomic<bool> m_shuffleAssign;
	std::vector<ThreadPtr> m_workers;
	std::vector<WorkerContextUPtr> m_workers_context;
	// The queue created by the shuffle stage (input of the reduce stage)
	std::vector<IntermediateVec> m_shuffle_queue;
};

#endif // JOB_CONTEXT_H