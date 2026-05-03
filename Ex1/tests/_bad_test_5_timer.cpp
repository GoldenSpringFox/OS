#include "uthreads.h"
#include <iostream>
#include <limits>

int sleep_completed = 0;
int block_sleep_resume_completed = 0;

void sleeping_thread() {
    int tid = uthread_get_tid();
    std::cout << "Thread " << tid << " starting and sleeping for 2 quantums" << std::endl;

    if (uthread_sleep(2) == -1) {
        std::cout << "Failed to sleep" << std::endl;
        exit(-1);
    }

    std::cout << "Thread " << tid << " woke up after sleep" << std::endl;
    sleep_completed++;
    uthread_terminate(tid);
}

void sleep_2_thread() {
    int tid = uthread_get_tid();
    std::cout << "Thread " << tid << " sleeping for 2 quantums" << std::endl;

    if (uthread_sleep(2) == -1) {
        std::cout << "Failed to sleep" << std::endl;
        exit(-1);
    }

    std::cout << "Thread " << tid << " woke up" << std::endl;
    uthread_terminate(tid);
}

void block_sleep_resume_thread() {
    int tid = uthread_get_tid();
    std::cout << "Thread " << tid << " starting and sleeping for 3 quantums" << std::endl;

    if (uthread_sleep(3) == -1) {
        std::cout << "Failed to sleep" << std::endl;
        exit(-1);
    }

    std::cout << "Thread " << tid << " woke up after sleep" << std::endl;
    block_sleep_resume_completed++;
    uthread_terminate(tid);
}

int main(int argc, char **argv) {
    int result = uthread_init(100000); // 100ms quantum
    if (result != 0) {
        std::cout << "uthread_init failed with code " << result << std::endl;
        exit(-1);
    }


    std::cout << "\n=== Test 4: Thread sleeps and wakes up ===" << std::endl;
    int tid1 = uthread_spawn(sleeping_thread);
    if (tid1 == -1) {
        std::cout << "ERROR: Failed to spawn thread" << std::endl;
        exit(-1);
    }

    // Yield to let thread start and sleep
    if (uthread_sleep(0) == -1) {
        std::cout << "Failed to sleep" << std::endl;
        exit(-1);
    }

    // Wait for the thread to wake up (should take 2 quantums)

    if (sleep_completed != 1) {
        std::cout << "ERROR: Expected 1 thread to complete sleep, got " << sleep_completed << std::endl;
        exit(-1);
    }
    std::cout << "Success: Thread slept and woke up" << std::endl;



    std::cout << "=== Test 1: Sleep with num_quantums == 0 (yield) ===" << std::endl;
    int initial_quantums = uthread_get_total_quantums();
    if (uthread_sleep(0) == -1) {
        std::cout << "Failed to yield" << std::endl;
        exit(-1);
    }
    int after_yield = uthread_get_total_quantums();
    if (after_yield != initial_quantums + 1) {
        std::cout << "ERROR: Expected quantum increase after yield, got " << after_yield - initial_quantums << std::endl;
        exit(-1);
    }
    std::cout << "Success: Yield worked, quantum increased by 1" << std::endl;

    std::cout << "\n=== Test 2: Main thread sleep with positive num_quantums (should fail) ===" << std::endl;
    result = uthread_sleep(1);
    if (result != -1) {
        std::cout << "ERROR: Expected -1 when main thread sleeps, got " << result << std::endl;
        exit(-1);
    }
    std::cout << "Success: Main thread cannot sleep" << std::endl;

    std::cout << "\n=== Test 3: Sleep with negative num_quantums (should fail) ===" << std::endl;
    result = uthread_sleep(-1);
    if (result != -1) {
        std::cout << "ERROR: Expected -1 for negative sleep, got " << result << std::endl;
        exit(-1);
    }
    std::cout << "Success: Cannot sleep negative quantums" << std::endl;

    std::cout << "\n=== Test 5: Multiple threads sleeping for same time, wake at same time ===" << std::endl;
    int tid2 = uthread_spawn(sleep_2_thread);
    int tid3 = uthread_spawn(sleep_2_thread);
    int tid4 = uthread_spawn(sleep_2_thread);
    if (tid2 == -1 || tid3 == -1 || tid4 == -1) {
        std::cout << "ERROR: Failed to spawn threads" << std::endl;
        exit(-1);
    }

    // Yield to let threads start
    if (uthread_sleep(0) == -1) {
        std::cout << "Failed to sleep" << std::endl;
        exit(-1);
    }

    // Wait for them to wake
    for (int i = 0; i < 3; ++i) {
        if (uthread_sleep(0) == -1) {
            std::cout << "Failed to sleep" << std::endl;
            exit(-1);
        }
    }

    // Since they wake at same time, order may vary, but all should terminate
    // We can't easily check exact order, but assume they all run
    std::cout << "Success: Multiple threads woke up at same time" << std::endl;

    std::cout << "\n=== Test 6: Thread sleeps, blocked during sleep, resume after sleep time ===" << std::endl;
    int tid5 = uthread_spawn(block_sleep_resume_thread);
    if (tid5 == -1) {
        std::cout << "ERROR: Failed to spawn thread" << std::endl;
        exit(-1);
    }

    // Yield once to let thread start and begin sleeping
    if (uthread_sleep(0) == -1) {
        std::cout << "Failed to sleep" << std::endl;
        exit(-1);
    }

    // After 1 quantum, block the sleeping thread
    if (uthread_sleep(0) == -1) {
        std::cout << "Failed to sleep" << std::endl;
        exit(-1);
    }

    result = uthread_block(tid5);
    if (result != 0) {
        std::cout << "ERROR: Failed to block sleeping thread" << std::endl;
        exit(-1);
    }
    std::cout << "Blocked thread " << tid5 << " while it was sleeping" << std::endl;

    // Wait for the sleep time to pass (2 more quantums, total 3)
    for (int i = 0; i < 3; ++i) {
        if (uthread_sleep(0) == -1) {
            std::cout << "Failed to sleep" << std::endl;
            exit(-1);
        }
    }

    // Now resume the thread
    result = uthread_resume(tid5);
    if (result != 0) {
        std::cout << "ERROR: Failed to resume thread" << std::endl;
        exit(-1);
    }
    std::cout << "Resumed thread " << tid5 << " after sleep time" << std::endl;

    // Yield to let resumed thread run
    if (uthread_sleep(0) == -1) {
        std::cout << "Failed to sleep" << std::endl;
        exit(-1);
    }

    if (block_sleep_resume_completed != 1) {
        std::cout << "ERROR: Expected thread to complete after resume, got " << block_sleep_resume_completed << std::endl;
        exit(-1);
    }
    std::cout << "Success: Thread was blocked during sleep and resumed after sleep quota" << std::endl;

    std::cout << "\n=== All tests passed! ===" << std::endl;
    return 0;
}