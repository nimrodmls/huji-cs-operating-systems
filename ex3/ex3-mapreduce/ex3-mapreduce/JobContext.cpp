#include <cassert>
#include <memory>
#include <algorithm>

#include "Common.h"
#include "JobContext.h"

JobContext::JobContext(
	const InputVec& inputVec, 
	OutputVec& outputVec, 
	const MapReduceClient& client,
	uint32_t worker_count) :

	m_inputVec(inputVec),
	m_outputVec(outputVec),
	m_client(client),
	m_shuffle_barrier(worker_count),
	m_shuffle_semaphore(0),
	m_output_mutex(),
	m_reduce_mutex(),
	m_stageCnt(0),
	m_shuffleAssign(false),
	m_workers()
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
	assert(UNDEFINED_STAGE == get_stage());

	// Creating the worker's context
	WorkerContext* worker_ctx = new WorkerContext(this);

	// Initialize the worker thread
	ThreadPtr worker = std::make_shared<Thread>(job_worker_thread, worker_ctx);
	m_workers.push_back(worker);
	m_workersContext.emplace_back(worker_ctx);
}

void JobContext::start_job()
{
	assert(UNDEFINED_STAGE == get_stage());

	set_stage(MAP_STAGE);
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
	// First loading the current stage and preserving it,
	// as it might change while this function runs
	// (e.g. the stage may change, and we would give out the wrong percentage)
	const uint64_t current_state = m_stageCnt.load();
	state->stage = Common::get_stage(current_state);
	const uint32_t total_entries = Common::get_stage_total(current_state);
	state->percentage = 100.0f *
		// Taking the minimum since the processed data can be "more" while the
		// threads are validating they're not out of bounds
		static_cast<float>(std::min(Common::get_stage_processed(current_state), total_entries)) / 
		static_cast<float>(total_entries);
}

void JobContext::add_output(K3* key, V3* value)
{
	m_output_mutex.lock();
	m_outputVec.push_back(std::make_pair(key, value));
	m_output_mutex.unlock();
}

stage_t JobContext::get_stage() const
{
	// The stage ID is stored in the 2 most significant bits of the counter
	return Common::get_stage(m_stageCnt.load());
}

uint32_t JobContext::get_stage_processed() const
{
	// The processed count is stored in the 31 least significant bits of the counter
	// (after the total count)
	return Common::get_stage_processed(m_stageCnt.load());
}

uint32_t JobContext::get_stage_total() const
{
	// The total count is stored in the 31 "middle" bits of the counter
	// (between the stage ID and the processed count)
	return Common::get_stage_total(m_stageCnt.load());
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
	m_stageCnt &= 0x7FFFFFFFULL | (0x3ULL << 62);
	// Set the new total count bits
	m_stageCnt |= (static_cast<uint64_t>(total) << 33);
}

bool JobContext::assign_shuffle_job()
{
	bool val = false;
	return m_shuffleAssign.compare_exchange_strong(val, true);
}

void JobContext::worker_handle_current_stage(WorkerContext* worker_ctx)
{
	JobContext* job_context = worker_ctx->jobContext;

	// Incrementing the processed count, and checking if the stage is complete
	uint32_t old_val = job_context->inc_stage_processed(1);
	// Stage is complete - Note that it's either complete if the processed
	// count is higher than the total count, or if the processed count overflowed 31 bits
	// (hence the stage total would be overwritten and corrupted)
	// TODO: Overflow check is incomplete
	while (old_val < job_context->get_stage_total())
	{
		switch (job_context->get_stage())
		{
		case MAP_STAGE:
			{
				const InputPair current_entry = job_context->get_input_vec()[old_val];
				job_context->get_client().map(
					current_entry.first, 
					current_entry.second,
					worker_ctx);
			}
			break;

		case REDUCE_STAGE:
			{
				job_context->m_reduce_mutex.lock();
				const IntermediateVec current_entry = job_context->m_shuffleQueue.back();
				job_context->m_shuffleQueue.pop_back();
				job_context->m_reduce_mutex.unlock();
				job_context->get_client().reduce(&current_entry, worker_ctx);
			}
			break;

		case SHUFFLE_STAGE:
			// fallthrough - shuffle is handled in a separate function
		case UNDEFINED_STAGE:
			// This should never happen
			break;
		}
		old_val = job_context->inc_stage_processed(1);
	}
}

void JobContext::worker_shuffle_stage(JobContext* job_context)
{
	// Resetting the processed count for the shuffle stage -
	// The total to process remains the same
	job_context->reset_stage_processed();
	
	IntermediateVec backPairs;
	uint32_t total_size = 0;
	// Getting all the pairs at the back of each of the worker's intermediates
	for (const auto& worker : job_context->m_workersContext)
	{
		IntermediateVec& vec = worker->intermediateVec;
		total_size += static_cast<uint32_t>(vec.size());
		backPairs.emplace_back(vec.back());
	}
	job_context->set_stage_total(total_size);

	while (!backPairs.empty())
	{
		// Extracting the maximal key value from the current back pairs, minimal is ignored
		const auto minmax = std::minmax_element(
			backPairs.begin(), 
			backPairs.end(),
			Common::key_less_than);
		IntermediatePair max_key = *minmax.second;

		// Finding all the pairs with the maximal key, in the intermediate vectors
		IntermediateVec all_key_pairs;
		for (const auto& worker : job_context->m_workersContext)
		{
			IntermediateVec& vec = worker->intermediateVec;
			while (!vec.empty() && Common::key_equals(vec.back(), max_key))
			{
				all_key_pairs.emplace_back(vec.back());
				vec.pop_back();
			}
		}

		job_context->inc_stage_processed(
			static_cast<uint32_t>(all_key_pairs.size()));

		// The new intermediate vector is ready
		job_context->m_shuffleQueue.push_back(all_key_pairs);

		backPairs.clear();
		for (const auto& worker : job_context->m_workersContext)
		{
			IntermediateVec& vec = worker->intermediateVec;
			if (!vec.empty())
			{
				backPairs.emplace_back(vec.back());
			}
		}
	} 
}

void* JobContext::job_worker_thread(void* context)
{
	assert(nullptr != context);

	WorkerContext* worker_ctx = static_cast<WorkerContext*>(context);
	JobContext* job_context = worker_ctx->jobContext;

	/* MAP STAGE */
	worker_handle_current_stage(worker_ctx);
	// The map stage has been completed, sort the intermediate vector according to the key
	std::sort(
		worker_ctx->intermediateVec.begin(), 
		worker_ctx->intermediateVec.end(), 
		Common::key_less_than
	);

	// Waiting on the barrier for all the workers to complete their map stage
	job_context->get_shuffle_barrier().barrier();

	// Once done - Starting the shuffle phase on one thread
	if (job_context->assign_shuffle_job())
	{
		// Shuffle stage is assigned to the current worker
		job_context->set_stage(SHUFFLE_STAGE);
		worker_shuffle_stage(job_context);

		// Shuffle is complete, allowing the beginning of the reduce stage
		job_context->set_stage(REDUCE_STAGE);
		job_context->reset_stage_processed();
		job_context->set_stage_total(static_cast<uint32_t>(job_context->m_shuffleQueue.size()));
	}
	else // Or waiting for the shuffle to end on the other threads
	{
		job_context->get_shuffle_semaphore().wait();
	}

	// Allowing all the workers to continue to the reduce stage
	job_context->get_shuffle_semaphore().post();

	worker_handle_current_stage(worker_ctx);

	return nullptr;
}
