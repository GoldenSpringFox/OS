/*
 * Initialize the virtual memory
 */
void VMinitialize(){
    PMwrite(0, 0);
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
    return pageTableStep(virtualPage, 0, 0);
}

uint64_t pageTableStep(uint64_t virtualPage, uint64_t currentFrame, uint64_t level){

    if (level == TABLES_DEPTH) {
        return currentFrame;
    } 
    uint64_t pageTableAddress = getPageTableAdressFromVirtual(virtualPage, level);
    uint64_t nextFrame;
    PMread(currentFrame+pageTableAddress, &nextFrame);

    return pageTableStep(virtualPage, nextFrame, level+1);
}


uint64_t getPageTableAdressFromVirtual(uint64_t virtualPage, uint64_t level) {
    return Page >> ((TABLES_DEPTH-level) * OFFSET_WIDTH) & ((1 << OFFSET_WIDTH) - 1);
}