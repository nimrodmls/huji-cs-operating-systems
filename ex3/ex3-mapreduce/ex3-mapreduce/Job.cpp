#include <cassert>
#include <memory>
#include <algorithm>

#include "Common.h"
#include "Job.h"

Job::Job(
	const InputVec& inputVec, 
	OutputVec& outputVec, 
	const MapReduceClient& client,
	uint32_t worker_count) :

	m_inputVec(inputVec),
	m_outputVec(outputVec),
	m_client(client),
	m_shuffle_barrier(worker_count),
	m_shuffle_semaphore(0),
	m_output_mutex(std::make_shared<Mutex>()),
	m_reduce_mutex(std::make_shared<Mutex>()),
	m_stage_status(0),
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

void Job::start_job()
{
	assert(UNDEFINED_STAGE == get_stage());

	set_stage(MAP_STAGE, static_cast<uint32_t>(m_inputVec.size()));
	for (const auto& worker : m_workers)
	{
		worker->run();
	}
}

void Job::wait()
{
	for (const auto& worker : m_workers)
	{
		worker->join();
	}
	// Since all the worker threads joined,
	// we can safely clear the workers vector
	m_workers.clear();
}

void Job::get_state(JobState* state) const
{
	// First loading the current stage and preserving it,
	// as it might change while this function runs
	// (e.g. the stage may change, and we would give out the wrong percentage)
	const uint64_t current_state = m_stage_status.load();
	state->stage = Common::get_stage(current_state);
	const uint32_t total_entries = Common::get_stage_total(current_state);
	state->percentage = 100.0f *
		// Taking the minimum since the processed data can be "more" while the
		// threads are validating they're not out of bounds
		static_cast<float>(std::min(Common::get_stage_processed(current_state), total_entries)) / 
		static_cast<float>(total_entries);
}

void Job::add_output(K3* key, V3* value)
{
	AutoMutexLock lock(m_output_mutex);
	m_outputVec.push_back(std::make_pair(key, value));
}

void Job::add_worker()
{
	// Allowing addition of workers only before the job has started
	assert(UNDEFINED_STAGE == get_stage());

	// Creating the worker's context
	WorkerContext* worker_ctx = new WorkerContext(this);

	// Initialize the worker thread
	ThreadPtr worker = std::make_shared<Thread>(job_worker_thread, worker_ctx);
	m_workers.push_back(worker);
	m_workers_context.emplace_back(worker_ctx);
}

stage_t Job::get_stage() const
{
	// The stage ID is stored in the 2 most significant bits of the counter
	return Common::get_stage(m_stage_status.load());
}

uint32_t Job::get_stage_total() const
{
	// The total count is stored in the 31 "middle" bits of the counter
	// (between the stage ID and the processed count)
	return Common::get_stage_total(m_stage_status.load());
}

void Job::set_stage(stage_t new_stage, uint32_t total)
{
	// Clear the stage bits
	//m_stage_status &= ~(0x3ULL << 62);
	// Set the new stage bits
	//m_stage_status |= (static_cast<uint64_t>(new_stage) << 62);
	m_stage_status = (static_cast<uint64_t>(new_stage) << 62) | (static_cast<uint64_t>(total) << 31);
}

uint32_t Job::inc_stage_processed(uint32_t val)
{
	// Increment the processed count
	return (m_stage_status.fetch_add(val) << 33) >> 33;
}

bool Job::assign_shuffle_job()
{
	bool val = false;
	return m_shuffleAssign.compare_exchange_strong(val, true);
}

void Job::worker_handle_current_stage(WorkerContext* worker_ctx)
{
	assert(nullptr != worker_ctx);

	Job* job_context = worker_ctx->jobContext;

	// Incrementing the processed count, and checking if the stage is complete
	uint32_t old_val = job_context->inc_stage_processed(1);
	while (old_val < job_context->get_stage_total())
	{
		switch (job_context->get_stage())
		{
		case MAP_STAGE:
			{
				const InputPair current_entry = job_context->m_inputVec[old_val];
				job_context->m_client.map(
					current_entry.first, 
					current_entry.second,
					worker_ctx);
			}
			break;

		case REDUCE_STAGE:
			{
				job_context->m_reduce_mutex->lock();
				const IntermediateVec current_entry = job_context->m_shuffle_queue.back();
				job_context->m_shuffle_queue.pop_back();
				job_context->m_reduce_mutex->unlock();
				job_context->m_client.reduce(&current_entry, worker_ctx);
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

void Job::worker_shuffle_stage(Job* job_context)
{
	assert(nullptr != job_context);
	
	IntermediateVec backPairs;
	uint32_t total_size = 0;
	// Getting all the pairs at the back of each of the worker's intermediates, only non-empty
	for (const auto& worker : job_context->m_workers_context)
	{
		IntermediateVec& vec = worker->intermediateVec;
		if (!vec.empty())
		{
			total_size += static_cast<uint32_t>(vec.size());
			backPairs.emplace_back(vec.back());
		}
	}

	job_context->set_stage(SHUFFLE_STAGE, total_size);

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
		for (const auto& worker : job_context->m_workers_context)
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
		job_context->m_shuffle_queue.push_back(all_key_pairs);

		backPairs.clear();
		for (const auto& worker : job_context->m_workers_context)
		{
			IntermediateVec& vec = worker->intermediateVec;
			if (!vec.empty())
			{
				backPairs.emplace_back(vec.back());
			}
		}
	} 
}

void* Job::job_worker_thread(void* context)
{
	assert(nullptr != context);

	WorkerContext* worker_ctx = static_cast<WorkerContext*>(context);
	Job* job_context = worker_ctx->jobContext;

	/*** MAP STAGE ***/
	worker_handle_current_stage(worker_ctx);
	// The map stage has been completed, sort the intermediate vector according to the key
	std::sort(
		worker_ctx->intermediateVec.begin(), 
		worker_ctx->intermediateVec.end(), 
		Common::key_less_than
	);

	// Waiting on the barrier for all the workers to complete their map stage
	job_context->m_shuffle_barrier.barrier();

	/*** SHUFFLE STAGE - ONE WORKER ONLY ***/
	// Once done - Starting the shuffle phase on one thread
	if (job_context->assign_shuffle_job())
	{
		// Shuffle stage is assigned to the current worker
		worker_shuffle_stage(job_context);

		// Shuffle is complete, allowing the beginning of the reduce stage
		job_context->set_stage(
			REDUCE_STAGE, static_cast<uint32_t>(job_context->m_shuffle_queue.size()));
	}
	else // Or waiting for the shuffle to end on the other threads
	{
		job_context->m_shuffle_semaphore.wait();
	}

	// Allowing all the workers to continue to the reduce stage
	job_context->m_shuffle_semaphore.post();

	/*** REDUCE STAGE ***/
	worker_handle_current_stage(worker_ctx);

	return nullptr;
}
