#ifndef JOB_CONTEXT_H
#define JOB_CONTEXT_H

#include <atomic>

#include "MapReduceFramework.h"
#include "Thread.h"

class JobContext;
struct WorkerContext
{
	uint32_t worker_id;
	// Reference to the owning job context
	JobContext* jobContext;
	// The worker's intermediate vector
	IntermediateVec intermediateVec;
};

/* Workaround for dealing with the fact that the MapReduceClient
 * is received in JobContext as a reference to abstract class 
 * It should be thread-safe since it allows only getting the pointer
 * and not modifying it post-creation */
class ClientWrapper
{
public:
	ClientWrapper(const MapReduceClient& client) :
		m_client(std::make_unique<MapReduceClient>(client))
	{}

	MapReduceClient* get() const
	{
		return m_client.get();
	}

private:
	const std::unique_ptr<MapReduceClient> m_client;
};

class JobContext
{
public:
	JobContext(
		const InputVec& inputVec, 
		OutputVec& outputVec, 
		const MapReduceClient& client);
	JobContext(const JobContext&) = delete;
	JobContext& operator=(const JobContext&) = delete;
	~JobContext() = default;

	// Adding a worker thread
	void add_worker();

	// Starting the job, by starting all the worker threads
	void start_job();

	// Waiting on the job to finish
	void wait() const;

	// Retreiving the current state of the job
	void get_state(JobState* state) const;

	stage_t get_stage() const;
	uint32_t get_stage_processed() const;
	uint32_t get_stage_total() const;

	void set_stage(stage_t new_stage);
	void reset_stage_processed();
	uint32_t inc_stage_processed();
	void set_stage_total(uint32_t total);

	bool assign_shuffle_job();

	InputVec& get_input_vec() { return m_inputVec; }
	const MapReduceClient& get_client() const { return m_client; }

private:

	/* Entrypoint for a job worker thread
	 * The worker thread will execute map-sort-reduce operations */
	static void* job_worker_thread(void* context);

	stage_t m_stage;
	InputVec m_inputVec;
	OutputVec m_outputVec;
	const MapReduceClient& m_client;
	/* The stage counter is a 64-bit bitfield, with the following structure:
	 * 31-bit processed entries counter, 31-bit total entries counter, 2-bit stage ID */
	std::atomic<uint64_t> m_stageCnt;
	// Boolean flag to indicate whether the shuffle job has been assigned to one of the workers
	std::atomic<bool> m_shuffleAssign;
	std::vector<ThreadPtr> m_workers;
	std::vector<IntermediateVec> m_workersIntermediate;
};

#endif // JOB_CONTEXT_H