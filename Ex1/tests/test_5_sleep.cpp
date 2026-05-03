#include "uthreads.h"
#include <iostream>
#include <limits>

int threads_woke_up = 0;
int block_sleep_test_completed = 0;

void sleeping_thread() {
    int tid = uthread_get_tid();
    std::cout << "Thread " << tid << " starting and sleeping for 2 quantums" << std::endl;

    if (uthread_sleep(2) == -1) {
        std::cout << "Failed to sleep" << std::endl;
        exit(-1);
    }

    std::cout << "Thread " << tid << " woke up after sleep" << std::endl;
    threads_woke_up++;
    uthread_terminate(tid);
}

void sleep_1_thread() {
    int tid = uthread_get_tid();
    std::cout << "Thread " << tid << " sleeping for 1 quantum" << std::endl;

    if (uthread_sleep(1) == -1) {
        std::cout << "Failed to sleep" << std::endl;
        exit(-1);
    }

    std::cout << "Thread " << tid << " woke up" << std::endl;
    threads_woke_up++;
    uthread_terminate(tid);
}

void sleep_3_thread() {
    int tid = uthread_get_tid();
    std::cout << "Thread " << tid << " sleeping for 3 quantums" << std::endl;

    if (uthread_sleep(3) == -1) {
        std::cout << "Failed to sleep" << std::endl;
        exit(-1);
    }

    std::cout << "Thread " << tid << " woke up" << std::endl;
    threads_woke_up++;
    uthread_terminate(tid);
}

void block_sleep_resume_thread() {
    int tid = uthread_get_tid();
    std::cout << "Thread " << tid << " sleeping for 2 quantums" << std::endl;

    if (uthread_sleep(2) == -1) {
        std::cout << "Failed to sleep" << std::endl;
        exit(-1);
    }

    std::cout << "Thread " << tid << " woke up after sleep" << std::endl;
    block_sleep_test_completed++;
    uthread_terminate(tid);
}

int main(int argc, char **argv) {
    int result;
    result = uthread_init(200000); // 200ms quantum
    if (result != 0) {
        std::cout << "uthread_init failed with code " << result << std::endl;
        exit(-1);
    }

    std::cout << "=== Test 1: Sleep with num_quantums == 0 (yield) ===" << std::endl;
    int initial_quantums = uthread_get_total_quantums();
    if (uthread_sleep(0) == -1) {
        std::cout << "Failed to yield" << std::endl;
        exit(-1);
    }
    int after_yield = uthread_get_total_quantums();
    if (after_yield != initial_quantums + 1) {
        std::cout << "ERROR: Expected quantum increase of 1, got " << (after_yield - initial_quantums) << std::endl;
        exit(-1);
    }
    std::cout << "Success: Yield worked, quantum increased by 1" << std::endl;

    std::cout << "\n=== Test 2: Main thread sleep with positive num_quantums (should fail) ===" << std::endl;
    result = uthread_sleep(1);
    if (result != -1) {
        std::cout << "ERROR: Expected -1 when main thread sleeps, got " << result << std::endl;
        exit(-1);
    }
    std::cout << "Success: Main thread cannot sleep with positive quantums" << std::endl;

    std::cout << "\n=== Test 3: Sleep with negative num_quantums (should fail) ===" << std::endl;
    result = uthread_sleep(-1);
    if (result != -1) {
        std::cout << "ERROR: Expected -1 for negative sleep, got " << result << std::endl;
        exit(-1);
    }
    std::cout << "Success: Cannot sleep with negative quantums" << std::endl;

    std::cout << "\n=== Test 4: Single thread sleeps and wakes up ===" << std::endl;
    int tid1 = uthread_spawn(sleeping_thread);
    if (tid1 == -1) {
        std::cout << "ERROR: Failed to spawn thread" << std::endl;
        exit(-1);
    }

    // Yield to let thread start and sleep
    uthread_sleep(0);

    // Wait for thread to complete
    for(;;) {
        if (threads_woke_up >= 1) {
            break;
        }
        uthread_sleep(0);
    }

    if (threads_woke_up != 1) {
        std::cout << "ERROR: Expected 1 thread to wake up, got " << threads_woke_up << std::endl;
        exit(-1);
    }
    std::cout << "Success: Single thread slept and woke up" << std::endl;

    std::cout << "\n=== Test 5: Multiple threads sleeping for different times ===" << std::endl;
    int tid2 = uthread_spawn(sleep_1_thread);
    int tid3 = uthread_spawn(sleep_3_thread);
    int tid4 = uthread_spawn(sleep_1_thread);
    if (tid2 == -1 || tid3 == -1 || tid4 == -1) {
        std::cout << "ERROR: Failed to spawn threads" << std::endl;
        exit(-1);
    }

    // Yield to let threads start
    uthread_sleep(0);

    // Wait for all threads to wake up
    for(;;) {
        if (threads_woke_up >= 4) {
            break;
        }
        uthread_sleep(0);
    }

    if (threads_woke_up != 4) {
        std::cout << "ERROR: Expected 4 threads to wake up, got " << threads_woke_up << std::endl;
        exit(-1);
    }
    std::cout << "Success: Multiple threads woke up at different times" << std::endl;

    std::cout << "\n=== Test 6: Thread sleeps, then blocked during sleep, then resumed after sleep time ===" << std::endl;
    int tid5 = uthread_spawn(block_sleep_resume_thread);
    if (tid5 == -1) {
        std::cout << "ERROR: Failed to spawn thread" << std::endl;
        exit(-1);
    }

    // Yield once to let thread start and begin sleeping
    uthread_sleep(0);

    // Now block the sleeping thread
    result = uthread_block(tid5);
    if (result != 0) {
        std::cout << "ERROR: Failed to block sleeping thread " << tid5 << std::endl;
        exit(-1);
    }
    std::cout << "Blocked thread " << tid5 << " during its sleep" << std::endl;

    // Wait a bit (yield several times)
    for (int i = 0; i < 4; ++i) {
        uthread_sleep(0);
    }

    // Now resume the thread after its sleep time should have elapsed
    result = uthread_resume(tid5);
    if (result != 0) {
        std::cout << "ERROR: Failed to resume thread " << tid5 << std::endl;
        exit(-1);
    }
    std::cout << "Resumed thread " << tid5 << " after sleep time passed" << std::endl;

    // Wait for thread to complete
    for(;;) {
        if (block_sleep_test_completed >= 1) {
            break;
        }
        uthread_sleep(0);
    }

    if (block_sleep_test_completed != 1) {
        std::cout << "ERROR: Expected blocked+slept thread to complete, got " << block_sleep_test_completed << std::endl;
        exit(-1);
    }
    std::cout << "Success: Thread was blocked during sleep, then resumed and completed" << std::endl;

    std::cout << "\n=== All tests passed! ===" << std::endl;
    return 0;
}
