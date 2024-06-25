#ifndef JOB_CONTEXT_H
#define JOB_CONTEXT_H

#include <atomic>
#include <queue>

#include "MapReduceFramework.h"
#include "Thread.h"
#include "Barrier.h"
#include "Mutex.h"
#include "Semaphore.h"

class JobContext;
class WorkerContext
{
public:
	WorkerContext(
		JobContext* job_context) :
		jobContext(job_context),
		intermediateVec()
	{}
	// Reference to the owning job context
	JobContext* jobContext;
	// The worker's intermediate vector
	IntermediateVec intermediateVec;
};

using WorkerContextUPtr = std::unique_ptr<WorkerContext>;

class JobContext
{
public:
	JobContext(
		const InputVec& inputVec, 
		OutputVec& outputVec, 
		const MapReduceClient& client,
		uint32_t worker_count);
	JobContext(const JobContext&) = delete;
	JobContext& operator=(const JobContext&) = delete;
	// The dtor is not waiting for the worker threads to finish
	// it is in the responsibility of the caller to wait for the job to finish
	~JobContext() = default;

	// Starting the job, by starting all the worker threads
	void start_job();

	// Waiting on the job to finish
	void wait() const;

	// Retreiving the current state of the job
	void get_state(JobState* state) const;

	void add_output(K3* key, V3* value);

	// TODO: Make these functions private
	stage_t get_stage() const;
	uint32_t get_stage_processed() const;
	uint32_t get_stage_total() const;

	void set_stage(stage_t new_stage);
	void reset_stage_processed();
	uint32_t inc_stage_processed(uint32_t val);
	void set_stage_total(uint32_t total);

	InputVec& get_input_vec() { return m_inputVec; }
	const MapReduceClient& get_client() const { return m_client; }
	Barrier& get_shuffle_barrier() { return m_shuffle_barrier; }
	Semaphore& get_shuffle_semaphore() { return m_shuffle_semaphore; }

private:
	// Adding a worker thread
	void add_worker();

	/* Atomically assigning the shuffle job
	 * Returns true if the shuffle job has been assigned to the caller,
	 * false otherwise */
	bool assign_shuffle_job();

	/* Entrypoint for a job worker thread
	 * The worker thread will execute map-sort-reduce operations */
	static void worker_handle_current_stage(
		WorkerContext* worker_ctx);

	static void worker_shuffle_stage(JobContext* job_context);

	static void* job_worker_thread(void* context);

	InputVec m_inputVec;
	OutputVec& m_outputVec;
	const MapReduceClient& m_client;
	Barrier m_shuffle_barrier;
	Semaphore m_shuffle_semaphore;
	// Mutex for synchronizing access to the output vector when reducing (add_output)
	Mutex m_output_mutex;
	// Mutex for synchronizing access to the shuffle queue when reducing (worker)
	Mutex m_reduce_mutex;
	/* The stage counter is a 64-bit bitfield, with the following structure:
	 * 31-bit processed entries counter, 31-bit total entries counter, 2-bit stage ID */
	std::atomic<uint64_t> m_stageCnt;
	// Boolean flag to indicate whether the shuffle job has been assigned to one of the workers
	std::atomic<bool> m_shuffleAssign;
	std::vector<ThreadPtr> m_workers;
	std::vector<WorkerContextUPtr> m_workersContext;
	std::vector<IntermediateVec> m_shuffleQueue;
};

#endif // JOB_CONTEXT_H