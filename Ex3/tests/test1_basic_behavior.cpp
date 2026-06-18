#include "VirtualMemory.h"
#include "PhysicalMemory.h"
#include <cassert>
#include <cstdint>
#include <iostream>

int main() {
    VMinitialize();

    const uint64_t firstAddress = 0;
    const uint64_t secondAddress = PAGE_SIZE + 5;
    const uint64_t thirdAddress = 3 * PAGE_SIZE + 11;

    assert(VMwrite(firstAddress, 0xA1B2C3D4) == 1);
    assert(VMwrite(secondAddress, 0x12345678) == 1);
    assert(VMwrite(thirdAddress, 0xCAFEBABE) == 1);

    word_t value = 0;
    assert(VMread(firstAddress, &value) == 1);
    assert(value == 0xA1B2C3D4);

    value = 0;
    assert(VMread(secondAddress, &value) == 1);
    assert(value == 0x12345678);

    value = 0;
    assert(VMread(thirdAddress, &value) == 1);
    assert(value == 0xCAFEBABE);

    std::cout << "test1_basic_behavior passed" << std::endl;
    return 0;
}
