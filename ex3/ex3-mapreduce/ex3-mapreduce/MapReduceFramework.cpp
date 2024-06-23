#include "MapReduceFramework.h"
#include "JobContext.h"

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