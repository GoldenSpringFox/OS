#include "PhysicalMemory.h"

/*
 * Initialize the virtual memory
 */
void VMinitialize(){
    for (uint64_t i=0; i<PAGE_SIZE; i++) {
        PMwrite(i, 0);
    }
}

/* reads a word from the given virtual address
 * and puts its content in *value.
 *
 * returns 1 on success.
 * returns 0 on failure (if the address cannot be mapped to a physical
 * address for any reason)
 */
int VMread(uint64_t virtualAddress, word_t* value){
    if (virtualAddress >= VIRTUAL_MEMORY_SIZE) {
        return 0;
    }

    uint64_t virtualPage = virtualAddress / PAGE_SIZE;
    uint64_t offset = virtualAddress % PAGE_SIZE;

    uint64_t frameIndex = VMgetMapping(virtualPage);
    if (frameIndex == 0) {
        PMrestore(0, virtualPage);
    }

    uint64_t physicalAddress = frameIndex * PAGE_SIZE + offset;
    PMread(physicalAddress, value);
    return 1;
}

/* Returns the physical frame index that virtualPage currently maps to,
 * or 0 if the page is not resident in RAM (never written or was evicted).
 * Does not allocate or restore anything — purely a read-only table walk.
 */
uint64_t VMgetMapping(uint64_t virtualPage){
    return stepThroughPageTable(virtualPage, 0, 0);
}

uint64_t stepThroughPageTable(uint64_t virtualPage, uint64_t currentFrame, uint64_t level){
    if (level == TABLES_DEPTH) {
        return currentFrame;
    } 

    uint64_t pageTableRow = decomposeVirtualAddress(virtualPage, level);
    word_t nextFrame;
    PMread(currentFrame + pageTableRow, &nextFrame);

    if (nextFrame == 0) {
        return 0;
    }

    return stepThroughPageTable(virtualPage, nextFrame, level+1);
}

uint64_t decomposeVirtualAddress(uint64_t virtualPage, uint64_t level) {
    return virtualPage >> ((TABLES_DEPTH-level) * OFFSET_WIDTH) & (PAGE_SIZE - 1);
}
