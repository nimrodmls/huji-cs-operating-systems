#include <cassert>

#include "MapReduceFramework.h"


#include "Job.h"

void emit2(K2* key, V2* value, void* context)
{
	assert(nullptr != context);

	WorkerContext* workerContext = static_cast<WorkerContext*>(context);
	workerContext->intermediateVec.push_back(std::make_pair(key, value));
}

void emit3(K3* key, V3* value, void* context)
{
	assert(nullptr != context);

	WorkerContext* workerContext = static_cast<WorkerContext*>(context);
	workerContext->jobContext->add_output(key, value);
}

JobHandle startMapReduceJob(
	const MapReduceClient& client,
	const InputVec& inputVec, 
	OutputVec& outputVec,
	int multiThreadLevel)
{
	Job* jobContext = new Job(
		inputVec, outputVec, client, multiThreadLevel);
	jobContext->start_job();

	return jobContext;
}

void waitForJob(JobHandle job)
{
	Job* jobContext = static_cast<Job*>(job);
	jobContext->wait();
}

void getJobState(JobHandle job, JobState* state)
{
	Job* jobContext = static_cast<Job*>(job);
	jobContext->get_state(state);
}

void closeJobHandle(JobHandle job)
{
	waitForJob(job);
	Job* jobContext = static_cast<Job*>(job);
	delete jobContext;
}