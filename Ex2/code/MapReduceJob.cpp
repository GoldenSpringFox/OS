#include "MapReduceJob.h"

/*
===============================================
Implement:
===============================================
*/

MapReduceJob::MapReduceJob(const MapReduceClient &client, const InputVec &inputVec, int multiThreadLevel)
    : client(client), inputVec(inputVec), preShuffleBarrier(multiThreadLevel)
{
    nextPairIndex = 0;
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
    while (true) {
        int index = nextPairIndex.fetch_add(1);
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
        std::unique_lock shuffleLock(shuffleMutex);
        isShuffleFinished = true;
        shuffleCV.notify_all();
    }

    // Reduce Phase
    nextPairIndex = 0;

    while (true) {
        int index = nextPairIndex.fetch_add(1);
        if (index >= shuffledVector.size()) break;
        client.reduce(shuffledVector[index], reduceContext);
    }
}

void MapReduceJob::ShuffleIntermediateVectors() 
{
    IntermediateVec sameKeyVector;
    
    bool areAllVectorsEmpty = false;
    while (!areAllVectorsEmpty)
    {
        std::shared_ptr maximumKey = threadMapContexts[0].getLastKey();
        std::vector<int> vectorWithMaxKeyIndexes;

        areAllVectorsEmpty = true;

        for (int i=0; i<threadMapContexts.size(); i++) {
            if (!threadMapContexts[i].isVectorEmpty()) {
                areAllVectorsEmpty = false;
            }
            
            std::shared_ptr currentKey = threadMapContexts[i].getLastKey();
            if (*maximumKey < *currentKey) {
                maximumKey = currentKey;
                vectorWithMaxKeyIndexes.clear();
                vectorWithMaxKeyIndexes.push_back(i);
            }
            if (areK2Equal(maximumKey, currentKey)) {
                vectorWithMaxKeyIndexes.push_back(i);
            }
        }

        if (sameKeyVector.size() != 0 && *maximumKey < *sameKeyVector[0].first) {
            shuffledVector.push_back(sameKeyVector);
            sameKeyVector.clear();
        }

        for (int i=0; i<vectorWithMaxKeyIndexes.size(); i++) {
            sameKeyVector.push_back(threadMapContexts[i].popLastPair());
        }
    }
}

bool areK2Equal(std::shared_ptr<K2> key1, std::shared_ptr<K2> key2) {
    return !(*key1 < *key2) && !(*key2 < *key1);
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
