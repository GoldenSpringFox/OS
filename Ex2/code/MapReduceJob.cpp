#include "MapReduceJob.h"

/*
===============================================
Implement:
===============================================
*/

MapReduceJob::MapReduceJob(const MapReduceClient &client, const InputVec &inputVec, int multiThreadLevel)
    : client(client), inputVec(inputVec), threadCount(multiThreadLevel),
      threadMapContexts(multiThreadLevel), nextPairIndex(0),
      preShuffleBarrier(multiThreadLevel), isShuffleFinished(false),
      doneThreadsCount(0), isCleanupDone(false)
{
	if (multiThreadLevel > 0)
	{
		threads.reserve(static_cast<size_t>(multiThreadLevel));
		for (int i = 0; i < multiThreadLevel; ++i)
		{
			threads.emplace_back(&MapReduceJob::MapReduceThread, this, i);
		}
	}
}

void setNewStage(MapReduceStage stage) {
    
}

void MapReduceJob::MapReduceThread(int threadId)
{
	threadMapContexts[threadId] = MapContext();

    // Map Phase
    while (true) {
        int index = nextPairIndex.fetch_add(1);
        if (index >= static_cast<int>(inputVec.size())) break;
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
        nextPairIndex = 0;
        isShuffleFinished = true;
        shuffleCV.notify_all();
    }

    // Reduce Phase
    while (true) {
        int index = nextPairIndex.fetch_add(1);
        if (index >= static_cast<int>(shuffledVector.size())) break;
        client.reduce(shuffledVector[index], reduceContext);
    }

    doneThreadsCount++;
}

bool areK2Equal(std::shared_ptr<K2> key1, std::shared_ptr<K2> key2) {
    return !(*key1 < *key2) && !(*key2 < *key1);
}

void MapReduceJob::ShuffleIntermediateVectors() 
{
    IntermediateVec sameKeyVector;
    
    bool areAllVectorsEmpty = false;
    while (!areAllVectorsEmpty)
    {
        std::shared_ptr<K2> maximumKey = nullptr;
        std::vector<int> vectorWithMaxKeyIndexes;

        areAllVectorsEmpty = true;

        for (int i=0; i < static_cast<int>(threadMapContexts.size()); i++) {
            if (threadMapContexts[i].isVectorEmpty()) {
                continue;
            }
            else {
                areAllVectorsEmpty = false;
            }

            if (maximumKey == nullptr) {
                maximumKey = threadMapContexts[i].getLastKey();
                vectorWithMaxKeyIndexes.push_back(i);
                continue;
            }
            
            std::shared_ptr currentKey = threadMapContexts[i].getLastKey();
            if (*maximumKey < *currentKey) {
                maximumKey = currentKey;
                vectorWithMaxKeyIndexes.clear();
                vectorWithMaxKeyIndexes.push_back(i);
            }
            else if (areK2Equal(maximumKey, currentKey)) {
                vectorWithMaxKeyIndexes.push_back(i);
            }
        }

        if (areAllVectorsEmpty) break;

        if (sameKeyVector.size() != 0 && *maximumKey < *sameKeyVector[0].first) {
            shuffledVector.push_back(sameKeyVector);
            sameKeyVector.clear();
        }

        for (int i=0; i<static_cast<int>(vectorWithMaxKeyIndexes.size()); i++) {
            sameKeyVector.push_back(threadMapContexts[vectorWithMaxKeyIndexes[i]].popLastPair());
        }
    }

    if (!sameKeyVector.empty()) {
        shuffledVector.push_back(sameKeyVector);
    }
}

MapReduceState MapReduceJob::getState(void) const
{
    // TODO: implement this function
}

void MapReduceJob::wait(void)
{
    std::unique_lock waitLock(waitMutex);
    if (isCleanupDone) return;

    for (std::thread &thread: threads) {
        thread.join();
    }
    isCleanupDone = true;
}

OutputVec MapReduceJob::getOutput(void)
{
    wait();

    OutputVec output = reduceContext.getOutputVector();
    std::sort(output.begin(), output.end(), [](const OutputPair &a, const OutputPair &b) {
            return *a.first < *b.first;
        });

    return output;
}

bool MapReduceJob::isDone(void) const
{
    return doneThreadsCount == threadCount;
}

MapReduceJob::~MapReduceJob()
{
    wait();
}
