#include <cassert>

#include "JobContext.h"

JobContext::JobContext(
	const InputVec& inputVec, 
	OutputVec& outputVec, 
	const MapReduceClient& client) :

	m_stage(UNDEFINED_STAGE),
	m_inputVec(inputVec),
	m_outputVec(outputVec),
	m_stageCnt(0),
	m_workers(),
	m_workersIntermediate()
{}

void JobContext::add_worker()
{
	// Creating the worker's context
	std::unique_ptr<WorkerContext> worker_ctx = std::make_unique<WorkerContext>();
	worker_ctx->jobContext = this;
	worker_ctx->worker_id = m_workers.size() - 1;
	worker_ctx->intermediateVec = IntermediateVec();

	// Initialize the worker thread
	ThreadPtr worker = std::make_shared<Thread>(job_worker_thread, worker_ctx.release());
	m_workers.push_back(worker);
}

void JobContext::start_job()
{
	assert(UNDEFINED_STAGE == m_stage);

	m_stage = MAP_STAGE;
	for (const auto& worker : m_workers)
	{
		worker->run();
	}
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

stage_t JobContext::get_stage() const
{
	// The stage ID is stored in the 2 most significant bits of the counter
	return static_cast<stage_t>(m_stageCnt.load() >> 62);
}

uint32_t JobContext::get_stage_processed() const
{
	// The processed count is stored in the 31 least significant bits of the counter
	// (after the total count)
	return (m_stageCnt.load() << 33) >> 33;
}

uint32_t JobContext::get_stage_total() const
{
	// The total count is stored in the 31 "middle" bits of the counter
	// (between the stage ID and the processed count)
	return (m_stageCnt.load() >> 33) & 0x7FFFULL;
}

void JobContext::set_stage(stage_t new_stage)
{
	// Clear the stage bits
	m_stageCnt &= ~(0x3ULL << 62);
	// Set the new stage bits
	m_stageCnt |= (static_cast<uint64_t>(new_stage) << 62);
}

uint32_t JobContext::inc_stage_processed()
{
	// Increment the processed count
	return (m_stageCnt++ << 33) >> 33; // (m_stageCnt.fetch_add(0x1ULL) << 33) >> 33;
}

void JobContext::set_stage_total(uint32_t total)
{
	// Clear the total count bits
	m_stageCnt &= ~(0x7FFFFFFFULL);
	// Set the new total count bits
	m_stageCnt |= (static_cast<uint64_t>(total) << 33);
}

void* JobContext::job_worker_thread(void* context)
{
	assert(nullptr != context);

	// Worker's context will be released upon thread termination
	const std::unique_ptr<WorkerContext> worker_ctx(static_cast<WorkerContext*>(context));
	JobContext* job_context = worker_ctx->jobContext;

	// Incrementing the processed count, and checking if the stage is complete
	uint32_t old_val = job_context->inc_stage_processed();
	// Stage is complete - Note that it's either complete if the processed
	// count is higher than the total count, or if the processed count overflowed 31 bits
	// (hence the stage total would be overwritten and corrupted)
	if ((old_val > job_context->get_stage_total()) || (0xFFFFFFFF >> 1) <= old_val)
	{
		
	}
	else // Stage is incomplete - More processing should be made
	{
		switch (job_context->get_stage())
		{
		case MAP_STAGE:

			break;
		case SHUFFLE_STAGE:
			break;
		case REDUCE_STAGE:
			break;
		case UNDEFINED_STAGE:
			// This should never happen
			break;
		}
	}

	return nullptr;
}
