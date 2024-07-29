#include <cassert>
#include <cstdlib>

#include "MapReduceFramework.h"
#include "Job.h"

/* Terminating the program & deleting the job triggered the exception */
static void terminate(Job* job)
{
	delete job;
	exit(1);
}

void emit2(K2* key, V2* value, void* context)
{
	try
	{
		assert(nullptr != context);

		WorkerContext* workerContext = static_cast<WorkerContext*>(context);
		workerContext->intermediateVec.push_back(std::make_pair(key, value));
	}
	catch (...)
	{
		terminate(static_cast<Job*>(context));
	}
}

void emit3(K3* key, V3* value, void* context)
{
	try
	{
		assert(nullptr != context);

		WorkerContext* workerContext = static_cast<WorkerContext*>(context);
		workerContext->jobContext->add_output(key, value);
	}
	catch (...)
	{
		terminate(static_cast<Job*>(context));
	}
}

JobHandle startMapReduceJob(
	const MapReduceClient& client,
	const InputVec& inputVec, 
	OutputVec& outputVec,
	int multiThreadLevel)
{
	Job* job_context = nullptr;
	try
	{
		job_context = new Job(
			inputVec, outputVec, client, multiThreadLevel);
		job_context->start_job();
	}
	catch (...)
	{
		terminate(job_context);
	}

	return job_context;
}

void waitForJob(JobHandle job)
{
	try
	{
		Job* jobContext = static_cast<Job*>(job);
		jobContext->wait();
	}
	catch (...)
	{
		terminate(static_cast<Job*>(job));
	}
}

void getJobState(JobHandle job, JobState* state)
{
	try
	{
		Job* jobContext = static_cast<Job*>(job);
		jobContext->get_state(state);
	}
	catch (...)
	{
		terminate(static_cast<Job*>(job));
	}
}

void closeJobHandle(JobHandle job)
{
	try
	{
		waitForJob(job);
		delete static_cast<Job*>(job);
	}
	catch (...)
	{
		// Will not attempt to delete if exception is raised during the procedure
		exit(1);
	}
}