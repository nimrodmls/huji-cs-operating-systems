#ifndef JOB_CONTEXT_H
#define JOB_CONTEXT_H

#include <atomic>
#include <queue>

#include "MapReduceFramework.h"
#include "Thread.h"
#include "Barrier.h"

// This abomination is a queue of pairs, of intermediate key and its
// correspoding vector of values collected from all the workers
using shuffle_queue = std::priority_queue<std::pair<K2*, std::vector<V2*>>>;

class JobContext;
struct WorkerContext
{
	// Reference to the owning job context
	JobContext* jobContext;
	// The worker's intermediate vector
	IntermediateVec intermediateVec;
};

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
	~JobContext() = default;

	// Starting the job, by starting all the worker threads
	void start_job();

	// Waiting on the job to finish
	void wait() const;

	// Retreiving the current state of the job
	void get_state(JobState* state) const;

	// TODO: Make these functions private
	stage_t get_stage() const;
	uint32_t get_stage_processed() const;
	uint32_t get_stage_total() const;

	void set_stage(stage_t new_stage);
	void reset_stage_processed();
	uint32_t inc_stage_processed();
	void set_stage_total(uint32_t total);

	InputVec& get_input_vec() { return m_inputVec; }
	const MapReduceClient& get_client() const { return m_client; }
	Barrier& get_shuffle_barrier() { return m_shuffle_barrier; }
	std::vector<IntermediateVec>& get_workers_intermediate() { return m_workersIntermediate; }

private:
	// Adding a worker thread
	void add_worker();

	/* Atomically assigning the shuffle job
	 * Returns true if the shuffle job has been assigned to the caller,
	 * false otherwise */
	bool assign_shuffle_job();

	/* Entrypoint for a job worker thread
	 * The worker thread will execute map-sort-reduce operations */
	static void* job_worker_thread(void* context);
	static void worker_shuffle_stage(const std::shared_ptr<WorkerContext> worker_ctx);
	static bool pair_compare(
		const IntermediatePair& elem1, 
		const IntermediatePair& elem2)
	{
		return elem1.first < elem2.first;
	}

	stage_t m_stage;
	InputVec m_inputVec;
	OutputVec m_outputVec;
	const MapReduceClient& m_client;
	Barrier m_shuffle_barrier;
	/* The stage counter is a 64-bit bitfield, with the following structure:
	 * 31-bit processed entries counter, 31-bit total entries counter, 2-bit stage ID */
	std::atomic<uint64_t> m_stageCnt;
	// Boolean flag to indicate whether the shuffle job has been assigned to one of the workers
	std::atomic<bool> m_shuffleAssign;
	std::vector<ThreadPtr> m_workers;
	std::vector<IntermediateVec> m_workersIntermediate;
	shuffle_queue m_shuffleQueue;
};

#endif // JOB_CONTEXT_H