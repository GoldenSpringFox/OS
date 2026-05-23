#ifndef REDUCE_CONTEXT_H
#define REDUCE_CONTEXT_H

#include "MapReduceKeys.h"
#include <mutex>

class ReduceContext
{
public:
    void addOutput(std::shared_ptr<K3> key, std::shared_ptr<V3> value);

    OutputVec getOutputVector();

private:
    OutputVec outputVector;
    std::mutex outputVectorMutex;
};

#endif // REDUCE_CONTEXT_H