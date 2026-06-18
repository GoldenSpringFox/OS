#include "PhysicalMemory.h"

/*
 * Initialize the virtual memory
 */
void VMinitialize(){
    resetPageTable(0);
}

void resetPageTable(uint64_t pageTableAddress) {
    for (uint64_t i=0; i<PAGE_SIZE; i++) {
        PMwrite(pageTableAddress * PAGE_SIZE + i, 0);
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
        frameIndex = handlePageFault(virtualPage);
    }

    uint64_t physicalAddress = frameIndex * PAGE_SIZE + offset;
    PMread(physicalAddress, value);
    return 1;
}

/* writes a word to the given virtual address
 *
 * returns 1 on success.
 * returns 0 on failure (if the address cannot be mapped to a physical
 * address for any reason)
 */

int VMwrite(uint64_t virtualAddress, word_t value)
{
    if (virtualAddress >= VIRTUAL_MEMORY_SIZE) {
        return 0;
    }

    uint64_t virtualPage = virtualAddress / PAGE_SIZE;
    uint64_t offset = virtualAddress % PAGE_SIZE;

    uint64_t frameIndex = VMgetMapping(virtualPage);
    if (frameIndex == 0) {
        frameIndex = handlePageFault(virtualPage);
    }

    uint64_t physicalAddress = frameIndex * PAGE_SIZE + offset;
    PMwrite(physicalAddress, value);
    return 1;
}


uint64_t handlePageFault(uint64_t virtualPage) {
    word_t currentAddress = 0;
    word_t nextAddress;
    uint64_t lastCreatedPageTable = 0;

    for (int level=0; level<TABLES_DEPTH; level++) {
        uint64_t pageTableRow = decomposeVirtualPage(virtualPage, level);
        PMread(currentAddress * PAGE_SIZE + pageTableRow, &nextAddress);

        if (nextAddress == 0) {
            VictimInformation victim = chooseVictim(virtualPage, lastCreatedPageTable);
            if (victim.needsEviction) {
                PMevict(victim.frame, victim.pageNumber);
            }
            
            if (level+1 < TABLES_DEPTH) {
                resetPageTable(victim.frame);
            }
            else {
                PMrestore(victim.frame, virtualPage);
            }

            PMwrite(currentAddress * PAGE_SIZE + pageTableRow, victim.frame);

            nextAddress = victim.frame;
        }

        currentAddress = nextAddress;
    }

    return currentAddress;
}

struct VictimInformation 
{
    uint64_t pageNumber;
    uint64_t frame;
    uint64_t parentFrame;
    bool needsEviction;
    uint64_t CyclicalDistance; 
};

VictimInformation chooseVictim(uint64_t virtualPage, uint64_t lastCreatedPageTable) {
    VictimInformation currentVictim;
    currentVictim->CyclicalDistance = VIRTUAL_MEMORY_SIZE; 
    uint64_t maxFrameIndex = 0;

    findVictimInPageTable(virtualPage, 0, 0, 0, &currentVictim, &maxFrameIndex);
}


void findVictimInPageTable(uint64_t virtualPage, uint64_t currentFrame, uint64_t level, 
    uint64_t currentPageNumber, VictimInformation* currentVictim, uint64_t* maxFrameIndex){
    if (currentFrame > *maxFrameIndex) {
        *maxFrameIndex = currentFrame;
    }
    if (level == TABLES_DEPTH) {
        // calculate cyclic distance
        uint64_t cyclicalDistance = calculateCyclicalDistance(virtualPage, *currentPageNumber);
        // compare to current victim
        if (cyclicalDistance > currentVictim->CyclicalDistance || 
            (currentPageNumber < currentVictim->pageNumber &&
            cyclicalDistance == currentVictim->CyclicalDistance) ) {
            currentVictim->CyclicalDistance = cyclicalDistance;
            currentVictim->pageNumber = virtualPage;
            currentVictim->frame = currentFrame;
            currentVictim->needsEviction = true;
        } 
        return;
    }
    else {
        bool isPageTableEmpty = true;
        for (int i = 0; i < PAGE_SIZE; i++) {
            uint64_t nextFrame; 
            PMread(currentFrame * PAGE_SIZE + i, &nextFrame);
            if (nextFrame != 0) {
                isPageTableEmpty = false;
                findVictimInPageTable(virtualPage, nextFrame, level + 1, 
                    currentPageNumber * PAGE_SIZE + i, currentVictim, maxFrameIndex);
            }
        }
    }
}

uint64_t calculateCyclicalDistance(uint64_t page1, uint64_t page2) {
    uint64_t distance = (page1 > page2) ? page1 - page2 : page2 - page1;
    return (distance > NUM_PAGES / 2) ? NUM_PAGES - distance : distance;
}

/* Returns the physical frame index that virtualPage currently maps to,
 * or 0 if the page is not resident in RAM (never written or was evicted).
 * Does not allocate or restore anything — purely a read-only table walk.
 */
uint64_t VMgetMapping(uint64_t virtualPage){
    return findInPageTable(virtualPage, 0, 0);
}

uint64_t findInPageTable(uint64_t virtualPage, uint64_t currentFrame, uint64_t level){
    if (level == TABLES_DEPTH) {
        return currentFrame;
    } 

    uint64_t pageTableRow = decomposeVirtualPage(virtualPage, level);
    word_t nextFrame;
    PMread(currentFrame + pageTableRow, &nextFrame);

    if (nextFrame == 0) {
        return 0;
    }

    return findInPageTable(virtualPage, nextFrame, level+1);
}

uint64_t decomposeVirtualPage(uint64_t virtualPage, uint64_t level) {
    return virtualPage >> ((TABLES_DEPTH-level) * OFFSET_WIDTH) & (PAGE_SIZE - 1);
}
