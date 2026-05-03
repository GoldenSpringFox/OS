#include "uthreads.h"
#include <iostream>
#include <limits>

int resume_count = 0;

void thread_that_yields() {
    int tid = uthread_get_tid();
    std::cout << "Thread " << tid << " starting" << std::endl;
    
    // Yield control
    if (uthread_sleep(0) == -1) {
        std::cout << "Failed to sleep" << std::endl;
        exit(-1);
    }
    
    std::cout << "Thread " << tid << " resumed after yield" << std::endl;
    uthread_terminate(tid);
}

void blocking_thread() {
    int tid = uthread_get_tid();
    std::cout << "Thread " << tid << " about to block itself" << std::endl;
    
    // Block self
    if (uthread_block(tid) == -1) {
        std::cout << "ERROR: Failed to block thread " << tid << std::endl;
        exit(-1);
    }
    
    // This should only execute if thread is resumed
    std::cout << "Thread " << tid << " was resumed!" << std::endl;
    resume_count++;
    
    // Terminate self
    if (uthread_terminate(tid) != 0) {
        std::cout << "ERROR: Failed to terminate thread " << tid << std::endl;
        exit(-1);
    }
}

int main(int argc, char **argv) {
    int result = uthread_init(std::numeric_limits<int>::max());
    if (result != 0) {
        std::cout << "uthread_init failed with code " << result << std::endl;
        exit(-1);
    }

    std::cout << "=== Test 1: Block main thread should fail ===" << std::endl;
    result = uthread_block(0);
    if (result != -1) {
        std::cout << "ERROR: Expected -1 when blocking main thread, got " << result << std::endl;
        exit(-1);
    }
    std::cout << "Success: Cannot block main thread" << std::endl;

    std::cout << "\n=== Test 2: Block non-existent thread should fail ===" << std::endl;
    result = uthread_block(999);
    if (result != -1) {
        std::cout << "ERROR: Expected -1 when blocking non-existent thread, got " << result << std::endl;
        exit(-1);
    }
    std::cout << "Success: Cannot block non-existent thread" << std::endl;

    std::cout << "\n=== Test 3: Resume non-existent thread should fail ===" << std::endl;
    result = uthread_resume(999);
    if (result != -1) {
        std::cout << "ERROR: Expected -1 when resuming non-existent thread, got " << result << std::endl;
        exit(-1);
    }
    std::cout << "Success: Cannot resume non-existent thread" << std::endl;

    std::cout << "\n=== Test 4: Spawn and block multiple threads ===" << std::endl;
    int tid1 = uthread_spawn(blocking_thread);
    int tid2 = uthread_spawn(blocking_thread);
    int tid3 = uthread_spawn(blocking_thread);
    
    if (tid1 == -1 || tid2 == -1 || tid3 == -1) {
        std::cout << "ERROR: Failed to spawn threads" << std::endl;
        exit(-1);
    }
    
    std::cout << "Spawned threads: " << tid1 << ", " << tid2 << ", " << tid3 << std::endl;

    // Yield to let threads block themselves
    std::cout << "\nMain thread yielding to let other threads block..." << std::endl;
    if (uthread_sleep(0) == -1) {
        std::cout << "Failed to sleep" << std::endl;
        exit(-1);
    }

    std::cout << "\n=== Test 5: Resume blocked threads ===" << std::endl;
    result = uthread_resume(tid1);
    if (result != 0) {
        std::cout << "ERROR: Failed to resume thread " << tid1 << std::endl;
        exit(-1);
    }
    std::cout << "Successfully resumed thread " << tid1 << std::endl;

    // Yield to let resumed thread run
    if (uthread_sleep(0) == -1) {
        std::cout << "Failed to sleep" << std::endl;
        exit(-1);
    }

    result = uthread_resume(tid2);
    if (result != 0) {
        std::cout << "ERROR: Failed to resume thread " << tid2 << std::endl;
        exit(-1);
    }
    std::cout << "Successfully resumed thread " << tid2 << std::endl;

    // Yield to let resumed thread run
    if (uthread_sleep(0) == -1) {
        std::cout << "Failed to sleep" << std::endl;
        exit(-1);
    }

    result = uthread_resume(tid3);
    if (result != 0) {
        std::cout << "ERROR: Failed to resume thread " << tid3 << std::endl;
        exit(-1);
    }
    std::cout << "Successfully resumed thread " << tid3 << std::endl;

    // Yield to let resumed thread run and terminate
    if (uthread_sleep(0) == -1) {
        std::cout << "Failed to sleep" << std::endl;
        exit(-1);
    }

    std::cout << "\n=== Test 6: Resume already running thread (should have no effect) ===" << std::endl;
    int tid4 = uthread_spawn(thread_that_yields);
    if (tid4 == -1) {
        std::cout << "ERROR: Failed to spawn thread" << std::endl;
        exit(-1);
    }

    // Yield to let thread run
    if (uthread_sleep(0) == -1) {
        std::cout << "Failed to sleep" << std::endl;
        exit(-1);
    }

    result = uthread_resume(tid4);
    if (result != 0) {
        std::cout << "ERROR: Failed to resume thread " << tid4 << std::endl;
        exit(-1);
    }
    std::cout << "Resume of running/ready thread had no effect (as expected)" << std::endl;

    // Yield to let thread complete
    if (uthread_sleep(0) == -1) {
        std::cout << "Failed to sleep" << std::endl;
        exit(-1);
    }

    std::cout << "\n=== Test 7: Verify resume_count ===" << std::endl;
    if (resume_count != 3) {
        std::cout << "ERROR: Expected 3 resumed threads, got " << resume_count << std::endl;
        exit(-1);
    }
    std::cout << "Success: All 3 blocked threads were successfully resumed" << std::endl;

    std::cout << "\n=== All tests passed! ===" << std::endl;
    return 0;
}
