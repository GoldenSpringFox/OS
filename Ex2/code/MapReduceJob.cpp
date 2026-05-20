#include "MapReduceJob.h"

/*
===============================================
Implement:
===============================================
*/

MapReduceJob::MapReduceJob(const MapReduceClient &client, const InputVec &inputVec, int multiThreadLevel)
    : client(client), inputVec(inputVec), preShuffleBarrier(multiThreadLevel)
{
    nextInputPairIndex = 0;
    isShuffleFinished = false;

	if (multiThreadLevel > 0)
	{
		threads.reserve(static_cast<size_t>(multiThreadLevel));
		for (int i = 0; i < multiThreadLevel; ++i)
		{
			threads.emplace_back(&MapReduceJob::MapReduceThread, this, i);
		}
	}
}

void MapReduceJob::MapReduceThread(int threadId)
{
	threadMapContexts[threadId] = MapContext();

    // Map Phase
    while (true) 
    {
        int index = nextInputPairIndex.fetch_add(1);
        if (index >= inputVec.size()) break;
        client.map(inputVec[index].first, inputVec[index].second, threadMapContexts[threadId]);
    }

    // Sort Phase
    threadMapContexts[threadId].sortIntermediateByKey();

    // Shuffle Phase
    preShuffleBarrier.arrive_and_wait();

    if (threadId != 0) {
        std::unique_lock shuffleLock(shuffleMutex);
        shuffleCV.wait(shuffleLock, [this]{ return this->isShuffleFinished; });
    }
    else {
        ShuffleIntermediateVectors();
    }

}

void MapReduceJob::ShuffleIntermediateVectors() 
{
    while (threadMapContexts.size() > 0) 
    {
        for (int i=0; i<threadMapContexts.size(); i++) {
            
        }
    }
}

MapReduceState MapReduceJob::getState(void) const
{
    // TODO: implement this function
}

void MapReduceJob::wait(void)
{
    // TODO: implement this function
}

OutputVec MapReduceJob::getOutput(void)
{
    // TODO: implement this function
}

bool MapReduceJob::isDone(void) const
{
    // TODO: implement this function
}

MapReduceJob::~MapReduceJob()
{
    // TODO: implement this destructor
}
