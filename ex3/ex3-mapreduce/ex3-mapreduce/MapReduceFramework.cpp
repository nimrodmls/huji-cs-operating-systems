#include "MapReduceFramework.h"
#include "Thread.h"

/* Entrypoint for a job worker thread
 * The worker thread will execute map-sort-reduce operations
 */
static void* jobWorkerThread(void* context)
{
	return nullptr;
}

JobHandle startMapReduceJob(
	const MapReduceClient& client,
	const InputVec& inputVec, 
	OutputVec& outputVec,
	int multiThreadLevel)
{
	return nullptr;
}

void waitForJob(JobHandle job)
{
	
}

void getJobState(JobHandle job, JobState* state)
{
	
}

void closeJobHandle(JobHandle job)
{
	
}