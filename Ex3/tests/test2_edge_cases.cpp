#include "VirtualMemory.h"
#include "PhysicalMemory.h"
#include <cassert>
#include <cstdint>
#include <iostream>

int main() {
    VMinitialize();

    word_t value = 0;

    assert(VMread(VIRTUAL_MEMORY_SIZE, &value) == 0);
    assert(VMwrite(VIRTUAL_MEMORY_SIZE, 42) == 0);
    assert(VMread(VIRTUAL_MEMORY_SIZE + 7, &value) == 0);
    assert(VMwrite(VIRTUAL_MEMORY_SIZE + 7, 99) == 0);

    const uint64_t unmappedPage = 17;
    assert(VMgetMapping(unmappedPage) == 0);

    const uint64_t mappedAddress = unmappedPage * PAGE_SIZE + 3;
    assert(VMwrite(mappedAddress, 0x55555555) == 1);
    assert(VMgetMapping(unmappedPage) != 0);

    value = 0;
    assert(VMread(mappedAddress, &value) == 1);
    assert(value == 0x55555555);

    std::cout << "test2_edge_cases passed" << std::endl;
    return 0;
}
