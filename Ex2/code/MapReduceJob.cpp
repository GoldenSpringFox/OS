#include "MapReduceJob.h"

/*
===============================================
Implement:
===============================================
*/

MapReduceJob::MapReduceJob(const MapReduceClient &client, const InputVec &inputVec, int multiThreadLevel)
    : client(client), inputVec(inputVec), threadCount(multiThreadLevel),
      threadMapContexts(multiThreadLevel), nextPairIndex(0),
      stageSwitchingBarrier(multiThreadLevel),
      preShuffleBarrier(multiThreadLevel), isShuffleFinished(false),
      doneThreadsCount(0), isCleanupDone(false)
{
    setNewStage(UNDEFINED_STAGE);
    
	if (multiThreadLevel > 0)
	{
		threads.reserve(static_cast<size_t>(multiThreadLevel));
		for (int i = 0; i < multiThreadLevel; ++i)
		{
			threads.emplace_back(&MapReduceJob::MapReduceThread, this, i);
		}
	}
}

void MapReduceJob::setNewStage(MapReduceStage stage) {
    int totalToProcess = 0;
    switch(stage) {
        case UNDEFINED_STAGE:
            totalToProcess = 0;
            break;
        case MAP_STAGE:
            totalToProcess = inputVec.size();
            break;
        case SHUFFLE_STAGE:
            totalToProcess = getIntermediatePairsCount();
            break;
        case REDUCE_STAGE:
            totalToProcess = static_cast<int>(shuffledVector.size());
            break;
    }

    uint64_t newStage = 0;
    newStage += static_cast<uint64_t>(totalToProcess) << 31;
    newStage += static_cast<uint64_t>(stage) << 62;
    currentState.store(newStage);
}

int MapReduceJob::getIntermediatePairsCount() {
    int sum = 0;
    for (int i=0; i < static_cast<int>(threadMapContexts.size()); i++) {
        sum += threadMapContexts[i].getIntermediatePairCount();
    }
    return sum;
}

void MapReduceJob::MapReduceThread(int threadId)
{
	threadMapContexts[threadId] = MapContext();
    // set map stage 
    if (threadId == 0) {
        setNewStage(MAP_STAGE);
    }
    //wait until thread 0 finish setting map stage
    stageSwitchingBarrier.arrive_and_wait();

    // Map Phase
    while (true) {
        int index = nextPairIndex.fetch_add(1);
        if (index >= static_cast<int>(inputVec.size())) break;
        client.map(inputVec[index].first, inputVec[index].second, threadMapContexts[threadId]);
        currentState.fetch_add(1);
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
        // thread 0 sets shuffle stage
        setNewStage(SHUFFLE_STAGE);
        ShuffleIntermediateVectors();

        std::unique_lock shuffleLock(shuffleMutex);
        nextPairIndex = 0;
        // set reduce stage
        setNewStage(REDUCE_STAGE);

        isShuffleFinished = true;
        shuffleCV.notify_all();
    }

    // Reduce Phase
    while (true) {
        int index = nextPairIndex.fetch_add(1);
        if (index >= static_cast<int>(shuffledVector.size())) break;
        client.reduce(shuffledVector[index], reduceContext);
        currentState.fetch_add(1);
    }

    doneThreadsCount.fetch_add(1);
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
            currentState.fetch_add(1);
        }
    }

    if (!sameKeyVector.empty()) {
        shuffledVector.push_back(sameKeyVector);
    }
}

MapReduceState MapReduceJob::getState(void) const
{
    // capture the current state atomically
    uint64_t state = currentState.load();
    
    MapReduceState mapReduceState;
    mapReduceState.stage = static_cast<MapReduceStage>(state >> 62);

    int totalToProcess = static_cast<int>((state >> 31) & 0x7FFFFFFF);

    int processed = static_cast<int>(state & 0x7FFFFFFF);

    // calculate percentage
    if (totalToProcess == 0) {
        mapReduceState.percentage = 0;
    }
    else {
        if (processed > totalToProcess) {
            processed = totalToProcess; 
        }
        mapReduceState.percentage = (static_cast<double>(processed) / totalToProcess) * 100.0;
    }
    return mapReduceState;
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
    return doneThreadsCount.load() == threadCount;
}

MapReduceJob::~MapReduceJob()
{
    wait();
}
