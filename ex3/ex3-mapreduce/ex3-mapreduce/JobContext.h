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

	MapReduceClient& get_client();
	stage_t get_stage() const;
	uint32_t get_stage_processed() const;
	uint32_t get_stage_total() const;

	void set_stage(stage_t new_stage);
	uint32_t inc_stage_processed();
	void set_stage_total(uint32_t total);

private:

	/* Entrypoint for a job worker thread
	 * The worker thread will execute map-sort-reduce operations */
	static void* job_worker_thread(void* context);

	stage_t m_stage;
	InputVec m_inputVec;
	OutputVec m_outputVec;
	MapReduceClient* m_client;
	/* The stage counter is a 64-bit bitfield, with the following structure:
	 * 31-bit processed entries counter, 31-bit total entries counter, 2-bit stage ID */
	std::atomic<uint64_t> m_stageCnt;
	std::vector<ThreadPtr> m_workers;
	std::vector<IntermediateVec> m_workersIntermediate;
};
