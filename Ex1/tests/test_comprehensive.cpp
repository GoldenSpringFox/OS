#include "uthreads.h"
#include <iostream>
#include <vector>

int test_count = 0;
int test_passed = 0;
int thread_started = 0;
int thread_ended = 0;
int thread_ran_count = 0;

#define ASSERT_EQ(actual, expected, msg) \
    do { \
        test_count++; \
        if ((actual) == (expected)) { \
            std::cout << "✓ Test " << test_count << ": " << msg << std::endl; \
            test_passed++; \
        } else { \
            std::cout << "✗ Test " << test_count << " FAILED: " << msg << " (expected " << (expected) << ", got " << (actual) << ")" << std::endl; \
        } \
    } while (0)

#define ASSERT_NE(actual, expected, msg) \
    do { \
        test_count++; \
        if ((actual) != (expected)) { \
            std::cout << "✓ Test " << test_count << ": " << msg << std::endl; \
            test_passed++; \
        } else { \
            std::cout << "✗ Test " << test_count << " FAILED: " << msg << " (should not be " << (expected) << ")" << std::endl; \
        } \
    } while (0)

void simple_thread() {
    int tid = uthread_get_tid();
    thread_ran_count++;
    uthread_terminate(tid);
}

void sleeping_thread() {
    int tid = uthread_get_tid();
    thread_started++;
    if (uthread_sleep(2) == -1) {
        std::cerr << "ERROR: sleep failed" << std::endl;
    }
    thread_ended++;
    uthread_terminate(tid);
}

void long_sleep_thread() {
    int tid = uthread_get_tid();
    if (uthread_sleep(5) == -1) {
        std::cerr << "ERROR: sleep failed" << std::endl;
    }
    uthread_terminate(tid);
}

void yield_thread() {
    int tid = uthread_get_tid();
    thread_ran_count++;
    if (uthread_sleep(0) == -1) {
        std::cerr << "ERROR: yield failed" << std::endl;
    }
    thread_ran_count++;
    uthread_terminate(tid);
}

void quantum_counter_thread() {
    int tid = uthread_get_tid();
    int start_quantums = uthread_get_quantums(tid);
    for (int i = 0; i < 3; i++) {
        if (uthread_sleep(0) == -1) {
            std::cerr << "ERROR: yield failed" << std::endl;
        }
    }
    int end_quantums = uthread_get_quantums(tid);
    std::cout << "  [Thread " << tid << " quantums: " << start_quantums << " -> " << end_quantums << "]" << std::endl;
    uthread_terminate(tid);
}

int main(int argc, char **argv) {
    std::cout << "\n========== COMPREHENSIVE THREAD LIBRARY TEST ==========" << std::endl;

    std::cout << "\n--- Test Group 1: Initialization ---" << std::endl;
    int result = uthread_init(100000);
    ASSERT_EQ(result, 0, "uthread_init with positive quantum");

    int initial_total = uthread_get_total_quantums();
    ASSERT_EQ(initial_total, 1, "Total quantums starts at 1");

    int main_tid = uthread_get_tid();
    ASSERT_EQ(main_tid, 0, "Main thread ID is 0");

    int main_quantums = uthread_get_quantums(0);
    ASSERT_EQ(main_quantums, 1, "Main thread quantums starts at 1");

    std::cout << "\n--- Test Group 2: Main Thread Sleep (negative cases) ---" << std::endl;
    result = uthread_sleep(1);
    ASSERT_NE(result, 0, "Main thread sleep(1) returns error");

    result = uthread_sleep(0);
    ASSERT_EQ(result, 0, "Main thread sleep(0) succeeds");

    result = uthread_sleep(-1);
    ASSERT_NE(result, 0, "Main thread sleep(-1) returns error");

    std::cout << "\n--- Test Group 3: Main Thread Block (negative cases) ---" << std::endl;
    result = uthread_block(0);
    ASSERT_NE(result, 0, "Cannot block main thread");

    std::cout << "\n--- Test Group 4: Spawning ---" << std::endl;
    thread_ran_count = 0;
    int tid1 = uthread_spawn(simple_thread);
    ASSERT_NE(tid1, -1, "Spawn simple thread");

    for (int i = 0; i < 3; i++) {
        uthread_sleep(0);
    }
    ASSERT_EQ(thread_ran_count, 1, "Simple thread ran");

    int tid_null = uthread_spawn(nullptr);
    ASSERT_EQ(tid_null, -1, "Spawn with NULL entry point fails");

    std::cout << "\n--- Test Group 5: Multiple Threads ---" << std::endl;
    thread_ran_count = 0;
    int tid2 = uthread_spawn(simple_thread);
    int tid3 = uthread_spawn(simple_thread);
    ASSERT_NE(tid2, -1, "Spawn second thread");
    ASSERT_NE(tid3, -1, "Spawn third thread");

    for (int i = 0; i < 5; i++) {
        uthread_sleep(0);
    }
    ASSERT_EQ(thread_ran_count, 2, "Both spawned threads ran");

    std::cout << "\n--- Test Group 6: Error Cases ---" << std::endl;
    int bad_quantums = uthread_get_quantums(999);
    ASSERT_EQ(bad_quantums, -1, "Get quantums of non-existent thread returns -1");

    result = uthread_terminate(999);
    ASSERT_EQ(result, -1, "Terminate non-existent thread returns -1");

    result = uthread_block(999);
    ASSERT_EQ(result, -1, "Block non-existent thread returns -1");

    result = uthread_resume(999);
    ASSERT_EQ(result, -1, "Resume non-existent thread returns -1");

    std::cout << "\n--- Test Group 7: Sleep and Wake ---" << std::endl;
    thread_started = 0;
    thread_ended = 0;
    int tid4 = uthread_spawn(sleeping_thread);
    ASSERT_NE(tid4, -1, "Spawn sleeping thread");

    uthread_sleep(0);
    ASSERT_EQ(thread_started, 1, "Sleeping thread started");

    for (int i = 0; i < 5; i++) {
        uthread_sleep(0);
    }
    ASSERT_EQ(thread_ended, 1, "Sleeping thread completed sleep");

    std::cout << "\n--- Test Group 8: Yield (sleep 0) ---" << std::endl;
    thread_ran_count = 0;
    int tid5 = uthread_spawn(yield_thread);
    ASSERT_NE(tid5, -1, "Spawn yield thread");

    for (int i = 0; i < 5; i++) {
        uthread_sleep(0);
    }
    ASSERT_EQ(thread_ran_count, 2, "Yield thread ran twice (before and after yield)");

    std::cout << "\n--- Test Group 9: Block and Resume ---" << std::endl;
    int tid6 = uthread_spawn(simple_thread);
    ASSERT_NE(tid6, -1, "Spawn thread to block");

    result = uthread_block(tid6);
    ASSERT_EQ(result, 0, "Block ready thread succeeds");

    thread_ran_count = 0;
    for (int i = 0; i < 3; i++) {
        uthread_sleep(0);
    }
    ASSERT_EQ(thread_ran_count, 0, "Blocked thread does not run");

    result = uthread_resume(tid6);
    ASSERT_EQ(result, 0, "Resume blocked thread succeeds");

    for (int i = 0; i < 3; i++) {
        uthread_sleep(0);
    }
    ASSERT_EQ(thread_ran_count, 1, "Resumed thread runs");

    std::cout << "\n--- Test Group 10: Block State Management ---" << std::endl;
    int tid7 = uthread_spawn(simple_thread);
    ASSERT_NE(tid7, -1, "Spawn thread for double-block test");

    uthread_block(tid7);
    result = uthread_block(tid7);
    ASSERT_EQ(result, 0, "Blocking already blocked thread returns 0");

    uthread_resume(tid7);
    for (int i = 0; i < 3; i++) {
        uthread_sleep(0);
    }

    std::cout << "\n--- Test Group 11: Resume ready/running ---" << std::endl;
    int tid8 = uthread_spawn(simple_thread);
    ASSERT_NE(tid8, -1, "Spawn thread for resume-ready test");

    result = uthread_resume(tid8);
    ASSERT_EQ(result, 0, "Resuming ready thread returns 0");

    for (int i = 0; i < 3; i++) {
        uthread_sleep(0);
    }

    std::cout << "\n--- Test Group 12: Block sleeping thread ---" << std::endl;
    int tid9 = uthread_spawn(long_sleep_thread);
    ASSERT_NE(tid9, -1, "Spawn long-sleep thread");

    uthread_sleep(0);
    result = uthread_block(tid9);
    ASSERT_EQ(result, 0, "Block sleeping thread succeeds");

    for (int i = 0; i < 6; i++) {
        uthread_sleep(0);
    }
    result = uthread_resume(tid9);
    ASSERT_EQ(result, 0, "Resume blocked-sleep thread succeeds");

    for (int i = 0; i < 3; i++) {
        uthread_sleep(0);
    }

    std::cout << "\n--- Test Group 13: Quantum Tracking ---" << std::endl;
    int total_before = uthread_get_total_quantums();
    int tid10 = uthread_spawn(quantum_counter_thread);
    ASSERT_NE(tid10, -1, "Spawn quantum counter thread");

    for (int i = 0; i < 5; i++) {
        uthread_sleep(0);
    }

    int total_after = uthread_get_total_quantums();
    ASSERT_NE(total_after, total_before, "Total quantums increased");

    std::cout << "\n--- Test Group 14: Multiple Sleepers ---" << std::endl;
    thread_ended = 0;
    int tid11 = uthread_spawn(sleeping_thread);
    int tid12 = uthread_spawn(sleeping_thread);
    int tid13 = uthread_spawn(sleeping_thread);
    ASSERT_NE(tid11, -1, "Spawn sleeper 1");
    ASSERT_NE(tid12, -1, "Spawn sleeper 2");
    ASSERT_NE(tid13, -1, "Spawn sleeper 3");

    for (int i = 0; i < 2; i++) {
        uthread_sleep(0);
    }
    for (int i = 0; i < 5; i++) {
        uthread_sleep(0);
    }
    ASSERT_EQ(thread_ended, 3, "All three sleeping threads woke up");

    std::cout << "\n--- Test Group 15: Self Termination ---" << std::endl;
    int tid14 = uthread_spawn(simple_thread);
    ASSERT_NE(tid14, -1, "Spawn thread for self-terminate");

    thread_ran_count = 0;
    for (int i = 0; i < 3; i++) {
        uthread_sleep(0);
    }
    ASSERT_EQ(thread_ran_count, 1, "Self-terminating thread ran and terminated");

    result = uthread_get_quantums(tid14);
    ASSERT_EQ(result, -1, "Terminated thread no longer exists");

    std::cout << "\n========== TEST SUMMARY ==========" << std::endl;
    std::cout << "Passed: " << test_passed << " / " << test_count << std::endl;
    if (test_passed == test_count) {
        std::cout << "\n✓ ALL TESTS PASSED!" << std::endl;
        return 0;
    }
    std::cout << "\n✗ SOME TESTS FAILED!" << std::endl;
    return 1;
}
