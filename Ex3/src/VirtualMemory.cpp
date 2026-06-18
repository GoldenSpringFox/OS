#include "VirtualMemory.h"
#include "PhysicalMemory.h"

struct VictimInformation {
    uint64_t pageNumber;
    uint64_t frame;
    uint64_t parentPageTableEntry;
    uint64_t cyclicalDistance;
    bool isPageTable;
};

// Forward declarations
void resetPageTable(uint64_t frame);
uint64_t VMgetMapping(uint64_t virtualPage);
uint64_t handlePageFault(uint64_t virtualPage);
uint64_t decomposeVirtualPage(uint64_t virtualPage, uint64_t level);
uint64_t chooseVictim(uint64_t virtualPage, uint64_t lastCreatedPageTable);
uint64_t calculateCyclicalDistance(uint64_t page1, uint64_t page2);
uint64_t findInPageTable(uint64_t virtualPage, uint64_t currentFrame, uint64_t level);
bool findVictimInPageTable(uint64_t virtualPage, uint64_t currentFrame, uint64_t parentPageTableEntry,
    uint64_t level, uint64_t currentPage, VictimInformation* currentVictim, uint64_t* maxFrameIndex);


/*
 * Initialize the virtual memory
 */
void VMinitialize(){
    resetPageTable(0);
}

void resetPageTable(uint64_t frame) {
    for (uint64_t i=0; i<PAGE_SIZE; i++) {
        PMwrite(frame * PAGE_SIZE + i, 0);
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

        if (nextAddress != 0) {
            currentAddress = nextAddress;
            continue;
        }

        uint64_t victimFrame = chooseVictim(virtualPage, lastCreatedPageTable);
        PMwrite(currentAddress * PAGE_SIZE + pageTableRow, victimFrame);

        if (level + 1 < TABLES_DEPTH) {
            resetPageTable(victimFrame);
        }
        else {
            PMrestore(victimFrame, virtualPage);
        }

        currentAddress = victimFrame;
    }

    return currentAddress;
}

uint64_t chooseVictim(uint64_t virtualPage, uint64_t lastCreatedPageTable) {
    VictimInformation victim;
    victim.cyclicalDistance = VIRTUAL_MEMORY_SIZE; 
    uint64_t maxFrameIndex = 0;

    findVictimInPageTable(virtualPage, 0, 0, 0, 0, &victim, &maxFrameIndex);

    if (victim.isPageTable) {
        PMwrite(victim.parentPageTableEntry, 0);
        return victim.frame;
    }
    
    if (maxFrameIndex + 1 < NUM_FRAMES) {
        return maxFrameIndex + 1;
    }

    PMevict(victim.frame, victim.pageNumber);
    PMwrite(victim.parentPageTableEntry, 0);

    return victim.frame;
}

/* return true to exit DFS early */
bool findVictimInPageTable(uint64_t virtualPage, uint64_t currentFrame, uint64_t parentPageTableEntry,
    uint64_t level, uint64_t currentPage, VictimInformation* currentVictim, uint64_t* maxFrameIndex){
    if (currentFrame > *maxFrameIndex) {
        *maxFrameIndex = currentFrame;
    }
    if (level == TABLES_DEPTH) {
        uint64_t cyclicalDistance = calculateCyclicalDistance(virtualPage, currentPage);
        
        if (cyclicalDistance > currentVictim->cyclicalDistance || (cyclicalDistance == currentVictim->cyclicalDistance && currentPage < currentVictim->pageNumber)) {
            currentVictim->cyclicalDistance = cyclicalDistance;
            currentVictim->pageNumber = currentPage;
            currentVictim->frame = currentFrame;
            currentVictim->parentPageTableEntry = parentPageTableEntry;
            currentVictim->isPageTable = false;
        } 
        return false;
    }
    else {
        bool isPageTableEmpty = true;
        for (int row = 0; row < PAGE_SIZE; row++) {
            word_t nextFrame;
            uint64_t pageTableEntry = currentFrame * PAGE_SIZE + row;
            PMread(pageTableEntry, &nextFrame);
            if (nextFrame != 0) {
                isPageTableEmpty = false;
                bool exitEarly = findVictimInPageTable(virtualPage, nextFrame, pageTableEntry,
                    level + 1, currentPage * PAGE_SIZE + row, currentVictim, maxFrameIndex);
                if (exitEarly) return true;
            }
        }

        if (isPageTableEmpty) {
            currentVictim->pageNumber = virtualPage;
            currentVictim->frame = currentFrame;
            currentVictim->parentPageTableEntry = parentPageTableEntry;
            currentVictim->isPageTable = true;
            return true;
        }
    }

    return false;
}

uint64_t calculateCyclicalDistance(uint64_t page1, uint64_t page2) {
    uint64_t distance = (page1 > page2) ? page1 - page2 : page2 - page1;
    return (distance > NUM_PAGES / 2) ? NUM_PAGES - distance : distance;
}

/* Returns the physical frame index that virtualPage currently maps to,
 * or 0 if the page is not resident in RAM (never written or was evicted).
 * Does not allocate or restore anything — purely a read-only table walk.
 */
uint64_t VMgetMapping(uint64_t virtualPage) {
    return findInPageTable(virtualPage, 0, 0);
}

uint64_t findInPageTable(uint64_t virtualPage, uint64_t currentFrame, uint64_t level){
    if (level == TABLES_DEPTH) {
        return currentFrame;
    } 

    uint64_t pageTableRow = decomposeVirtualPage(virtualPage, level);
    word_t nextFrame;
    PMread(currentFrame * PAGE_SIZE + pageTableRow, &nextFrame);

    if (nextFrame == 0) {
        return 0;
    }

    return findInPageTable(virtualPage, nextFrame, level+1);
}

uint64_t decomposeVirtualPage(uint64_t virtualPage, uint64_t level) {
    return virtualPage >> ((TABLES_DEPTH-level) * OFFSET_WIDTH) & (PAGE_SIZE - 1);
}
