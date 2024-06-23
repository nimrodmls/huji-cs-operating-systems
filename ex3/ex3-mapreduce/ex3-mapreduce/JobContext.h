#include <atomic>

#include "MapReduceFramework.h"
#include "Thread.h"

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
	JobContext(const InputVec& inputVec, 
		OutputVec& outputVec, 
		const MapReduceClient& client);
	JobContext(const JobContext&) = delete;
	JobContext& operator=(const JobContext&) = delete;
	~JobContext() = default;

	// Adding a worker thread
	void add_worker();

	// Waiting on the job to finish
	void wait() const;

	// Retreiving the current state of the job
	void get_state(JobState* state) const;

private:

	/* Entrypoint for a job worker thread
	 * The worker thread will execute map-sort-reduce operations */
	static void* job_worker_thread(void* context);

	enum stage_t m_stage;
	InputVec m_inputVec;
	OutputVec m_outputVec;
	std::atomic<uint64_t> m_inputIndex;
	std::vector<ThreadPtr> m_workers;
	std::vector<IntermediateVec> m_workersIntermediate;
};
