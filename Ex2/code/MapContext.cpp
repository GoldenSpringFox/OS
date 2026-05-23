#include "MapContext.h"

// implement here your constructor and destructor

void MapContext::addIntermediate(std::shared_ptr<K2> key, std::shared_ptr<V2> value)
{
    intermediateVector.push_back(IntermediatePair(key, value));
}

void MapContext::sortIntermediateByKey()
{
    std::sort(intermediateVector.begin(), intermediateVector.end(),
        [](const IntermediatePair &a, const IntermediatePair &b) {
            return *a.first < *b.first;
        }
    );
}

std::shared_ptr<K2> MapContext::getLastKey() {
    return intermediateVector.back().first;
}

IntermediatePair MapContext::popLastPair() {
    IntermediatePair lastPair = intermediateVector.back();
    intermediateVector.pop_back();
    return lastPair;
}

bool MapContext::isVectorEmpty() {
    return intermediateVector.empty();
}

int MapContext::getIntermediatePairCount() {
    return static_cast<int>(intermediateVector.size());
}
