#include "uthreads.h"
#include <iostream>
#include <limits>

bool sleep_completed = false;

void sleeping_thread() {
    int tid = uthread_get_tid();
    std::cout << "Thread " << tid << " starting and sleeping for 2 quantums" << std::endl;

    if (uthread_sleep(2) == -1) {
        std::cout << "Failed to sleep" << std::endl;
        exit(-1);
    }

    std::cout << "Thread " << tid << " woke up after sleep" << std::endl;
    sleep_completed = true;
    uthread_terminate(tid);
}

int main(int argc, char **argv) {
    int result;
    result = uthread_init(200000);
    if (result != 0) {
        std::cout << "uthread_init failed with code " << result << std::endl;
        exit(-1);
    }

    std::cout << "Creating a thread" << std::endl;
    result = uthread_spawn(sleeping_thread);
    if (result == -1) {
        std::cout << "Failed to create thread" << std::endl;
        exit(-1);
    }

    uthread_sleep(0);

    std::cout << "Waiting for thread to complete sleep" << std::endl;
    for(;;) {
        if (sleep_completed) {
            std::cout << "Success: Thread slept and woke up" << std::endl;
            break;
        }
        uthread_sleep(0);
    }
    
    std::cout << "Done" << std::endl;
}
