#include "JobContext.h"

JobContext::JobContext(
	const InputVec& inputVec, 
	OutputVec& outputVec, 
	const MapReduceClient& client) :

	m_stage(UNDEFINED_STAGE),
	m_inputVec(inputVec),
	m_outputVec(outputVec),
	m_inputIndex(0),
	m_workers(),
	m_workersIntermediate()
{}

void JobContext::add_worker()
{
	ThreadPtr worker = std::make_shared<Thread>(job_worker_thread);
	m_workers.push_back(worker);
	std::unique_ptr<WorkerContext> worker_ctx = std::make_unique<WorkerContext>();
	worker->run(worker_ctx.release());
	m_stage = MAP_STAGE;
}

void JobContext::wait() const
{
	for (const auto& worker : m_workers)
	{
		worker->join();
	}
}

void JobContext::get_state(JobState* state) const
{
	state->stage = m_stage;
	// TODO: Implement the percentage calculation
	state->percentage = 0.0f;
}

void* JobContext::job_worker_thread(void* context)
{
	std::unique_ptr<WorkerContext> worker_ctx(static_cast<WorkerContext*>(context));


	return nullptr;
}
