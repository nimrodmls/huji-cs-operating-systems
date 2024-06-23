#include "MapReduceFramework.h"
#include "JobContext.h"

void emit2(K2* key, V2* value, void* context)
{
}

void emit3(K3* key, V3* value, void* context)
{
}

JobHandle startMapReduceJob(
	const MapReduceClient& client,
	const InputVec& inputVec, 
	OutputVec& outputVec,
	int multiThreadLevel)
{
	JobContext* jobContext = new JobContext(inputVec, outputVec, client);
	for (int32_t idx = 0; idx < multiThreadLevel; ++idx)
	{
		jobContext->add_worker();
	}
	jobContext->start_job();

	return jobContext;
}

void waitForJob(JobHandle job)
{
	JobContext* jobContext = static_cast<JobContext*>(job);
	jobContext->wait();
}

void getJobState(JobHandle job, JobState* state)
{
	JobContext* jobContext = static_cast<JobContext*>(job);
	jobContext->get_state(state);
}

void closeJobHandle(JobHandle job)
{
	JobContext* jobContext = static_cast<JobContext*>(job);
	delete jobContext;
}