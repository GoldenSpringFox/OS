#ifndef MAP_REDUCE_JOB_H
#define MAP_REDUCE_JOB_H

#include "MapReduceClient.h"
#include <thread>
#include <atomic>
#include <vector>
#include <algorithm>
#include <barrier>
#include <condition_variable>

// you can add other includes here

enum MapReduceStage
{
	UNDEFINED_STAGE, // 0
	MAP_STAGE, // 1
	SHUFFLE_STAGE, // 2
	REDUCE_STAGE // 3
};

class MapReduceState
{
public:
	MapReduceStage stage;
	double percentage;

	inline bool operator==(const MapReduceState &other) const
	{
		return this->stage == other.stage && std::abs(this->percentage - other.percentage) < 1e-6;
	}

	inline bool operator!=(const MapReduceState &other) const
	{
		return !(*this == other);
	}
};

class MapReduceJob
{
public:
	/*
	You CAN NOT change or add properties to this part (public API).
	*/

	MapReduceJob(const MapReduceClient &client, const InputVec &inputVec, int multiThreadLevel);

	~MapReduceJob();

	MapReduceState getState(void) const;

	bool isDone(void) const;
	
	void wait(void);

	OutputVec getOutput(void);

private:
	const MapReduceClient &client;
	const InputVec &inputVec;
	const int threadCount;

	std::vector<std::thread> threads;
	std::vector<MapContext> threadMapContexts;

	std::atomic<int> nextPairIndex;

	std::barrier<> preShuffleBarrier;
	std::condition_variable shuffleCV;
	std::mutex shuffleMutex;
	bool isShuffleFinished;

	std::vector<IntermediateVec> shuffledVector;

	ReduceContext reduceContext;

	std::atomic<int> doneThreadsCount;

	std::mutex waitMutex;
	bool isCleanupDone;

	std::atomic<uint64_t> currentState;

	void MapReduceThread(int threadId);
	void ShuffleIntermediateVectors();
	bool areK2Equal(std::shared_ptr<K2> key1, std::shared_ptr<K2> key2);
	void setNewStage(MapReduceStage stage);
};
	
#endif // MAP_REDUCE_JOB_H
