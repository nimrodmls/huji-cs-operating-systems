#include <cassert>

#include "JobContext.h"

JobContext::JobContext(
	const InputVec& inputVec, 
	OutputVec& outputVec, 
	const MapReduceClient& client,
	uint32_t worker_count) :

	m_stage(UNDEFINED_STAGE),
	m_inputVec(inputVec),
	m_outputVec(outputVec),
	m_client(client),
	m_shuffle_barrier(worker_count),
	m_stageCnt(0),
	m_shuffleAssign(false),
	m_workers(),
	m_workersIntermediate()
{
	assert(0 < worker_count);
	assert(!inputVec.empty());

	for (uint32_t idx = 0; idx < worker_count; ++idx)
	{
		add_worker();
	}
}

void JobContext::add_worker()
{
	// Allowing addition of workers only before the job has started
	assert(UNDEFINED_STAGE == m_stage);

	// Creating the worker's context
	std::unique_ptr<WorkerContext> worker_ctx = std::make_unique<WorkerContext>();
	worker_ctx->jobContext = this;
	worker_ctx->intermediateVec = IntermediateVec();
	m_workersIntermediate.push_back(worker_ctx->intermediateVec);

	// Initialize the worker thread
	ThreadPtr worker = std::make_shared<Thread>(job_worker_thread, worker_ctx.release());
	m_workers.push_back(worker);
}

void JobContext::start_job()
{
	assert(UNDEFINED_STAGE == m_stage);

	m_stage = MAP_STAGE;
	set_stage_total(static_cast<uint32_t>(m_inputVec.size()));
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

void JobContext::reset_stage_processed()
{
	// Reset the processed count
	m_stageCnt &= ~(0x7FFFFFFFULL);
}

uint32_t JobContext::inc_stage_processed(uint32_t val)
{
	// Increment the processed count
	return (m_stageCnt.fetch_add(val) << 33) >> 33;
}

void JobContext::set_stage_total(uint32_t total)
{
	// Clear the total count bits
	m_stageCnt &= ~(0x7FFFFFFFULL);
	// Set the new total count bits
	m_stageCnt |= (static_cast<uint64_t>(total) << 33);
}

bool JobContext::assign_shuffle_job()
{
	bool val = false;
	return m_shuffleAssign.compare_exchange_strong(val, true);
}

void worker_handle_current_stage(const std::shared_ptr<WorkerContext> worker_ctx)
{
	JobContext* job_context = worker_ctx->jobContext;

	// Incrementing the processed count, and checking if the stage is complete
	uint32_t old_val = job_context->inc_stage_processed(1);
	// Stage is complete - Note that it's either complete if the processed
	// count is higher than the total count, or if the processed count overflowed 31 bits
	// (hence the stage total would be overwritten and corrupted)
	// TODO: Overflow check is incomplete
	while ((old_val < job_context->get_stage_total()) && (0xFFFFFFFF >> 1) > old_val)
	{
		switch (job_context->get_stage())
		{
		case MAP_STAGE:
			{
				const InputPair current_entry = job_context->get_input_vec()[old_val + 1];
				job_context->get_client().map(
					current_entry.first, 
					current_entry.second, 
					worker_ctx.get());
			}
			break;
		case SHUFFLE_STAGE:
			break;
		case REDUCE_STAGE:
			break;
		case UNDEFINED_STAGE:
			// This should never happen
			break;
		}
		old_val = job_context->inc_stage_processed(1);
	}

	// From this line - The current stage is finished and we should do post-processing
}

void JobContext::worker_shuffle_stage(JobContext* job_context)
{
	std::vector<IntermediateVec> workersIntermediate = 
		job_context->get_workers_intermediate();

	// Resetting the processed count for the shuffle stage -
	// The total to process remains the same
	job_context->reset_stage_processed();
	
	IntermediateVec backPairs;
	// Expecting at least 1 non-empty worker intermediate vector,
	// otherwise we will insert an empty vector into the shuffle queue
	// this is probably a safe assumption as it means that the input is empty.
	do
	{
		// Getting all the pairs at the back of each of the worker's intermediates
		for (IntermediateVec& vec : workersIntermediate)
		{
			backPairs.emplace_back(vec.back());
		}

		// Extracting the maximal key value from the current back pairs, minimal is ignored
		const auto minmax = std::minmax_element(
			backPairs.begin(), 
			backPairs.end(),
			JobContext::pair_compare);
		IntermediatePair max_key = *minmax.second;

		// Finding all the pairs with the maximal key, in the intermediate vectors
		IntermediateVec all_key_pairs;
		for (IntermediateVec& vec : workersIntermediate)
		{
			while (!vec.empty() && vec.back() < max_key)
			{
				all_key_pairs.emplace_back(vec.back());
				vec.pop_back();
			}
		}

		job_context->inc_stage_processed(
			static_cast<uint32_t>(all_key_pairs.size()));

		// The new intermediate vector is ready
		job_context->m_shuffleQueue.push(backPairs);

	} while (!backPairs.empty());
}

void* JobContext::job_worker_thread(void* context)
{
	assert(nullptr != context);

	// Worker's context will be released upon thread termination
	const std::shared_ptr<WorkerContext> worker_ctx(static_cast<WorkerContext*>(context));
	JobContext* job_context = worker_ctx->jobContext;

	/* MAP STAGE */
	worker_handle_current_stage(worker_ctx);
	// The map stage has been completed, sort the intermediate vector according to the key
	std::sort(
		worker_ctx->intermediateVec.begin(), 
		worker_ctx->intermediateVec.end(), 
		JobContext::pair_compare
	);

	// Waiting on the barrier for all the workers to complete their map stage
	job_context->get_shuffle_barrier().barrier();
	if (job_context->assign_shuffle_job())
	{
		// Shuffle stage is assigned to the current worker
		job_context->set_stage(SHUFFLE_STAGE);
		worker_shuffle_stage(job_context);

		// Shuffle is complete, allowing the beginning of the reduce stage
		job_context->set_stage(REDUCE_STAGE);
	}

	// Do reduce stage

	return nullptr;
}
