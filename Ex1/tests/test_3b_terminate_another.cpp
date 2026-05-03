#include "uthreads.h"
#include <iostream>
#include <limits>


void terminate_another_thread() {
    int tid = uthread_get_tid();
    if (tid == -1) {
        std::cout << "Failed to get tid" << std::endl;
        exit(-1);
    }
    
    if (tid != 1) {
        std::cout << "(quantums: " << uthread_get_total_quantums() << "," << uthread_get_quantums(tid) << ") Thread " << uthread_get_tid() << " waiting for thread 1" << std::endl;
        uthread_sleep(0);
    }

    if (tid == 1) {
        std::cout << "(quantums: " << uthread_get_total_quantums() << "," << uthread_get_quantums(tid) << ") Thread " << uthread_get_tid() << " Kills Thread 2" << std::endl;
        int terminate_result = uthread_terminate(2);
        if (terminate_result == -1) {
            std::cout << "Failed to terminate thread 2" << std::endl;
            exit(-1);
        }
    }

    if (tid != 0) {
        std::cout << "(quantums: " << uthread_get_total_quantums() << "," << uthread_get_quantums(tid) << ") Thread " << uthread_get_tid() << " waiting for thread 0" << std::endl;
        uthread_sleep(0);
    }

    if (tid == 0) {
        std::cout << "(quantums: " << uthread_get_total_quantums() << "," << uthread_get_quantums(tid) << ") Thread " << uthread_get_tid() << " Kills Thread 1" << std::endl;
        int terminate_result = uthread_terminate(1);
        if (terminate_result == -1) {
            std::cout << "Failed to terminate thread 1" << std::endl;
            exit(-1);
        }
    }

    if (tid != 3) {
        std::cout << "(quantums: " << uthread_get_total_quantums() << "," << uthread_get_quantums(tid) << ") Thread " << uthread_get_tid() << " waiting for thread 3" << std::endl;
        uthread_sleep(0);
    }

    if (tid == 3) {
        std::cout << "(quantums: " << uthread_get_total_quantums() << "," << uthread_get_quantums(tid) << ") Thread 3 Kills itself" << std::endl;
        int terminate_result = uthread_terminate(3);
        if (terminate_result == -1) {
            std::cout << "Failed to terminate thread 3" << std::endl;
            exit(-1);
        }
    }

    std::cout << "(quantums: " << uthread_get_total_quantums() << "," << uthread_get_quantums(tid) << ") Thread " << uthread_get_tid() << " sleeps before exiting" << std::endl;
    uthread_sleep(0);
    
    if (tid != 0) {
        std::cout << "(quantums: " << uthread_get_total_quantums() << "," << uthread_get_quantums(tid) << ") Thread " << uthread_get_tid() << " still running" << std::endl;
    }
}

int main(int argc, char **argv) {
    int result;
    result = uthread_init(std::numeric_limits<int>::max());
    if (result != 0) {
        std::cout << "uthread_init failed with code " << result << std::endl;
        exit(-1);
    }

    if (uthread_get_total_quantums() != 1) {
        std::cout << "Total quantums should be 1 after initialization, but got " << uthread_get_total_quantums() << std::endl;
        exit(-1);
    }

    std::cout << "Creating 3 threads" << std::endl;
    result = uthread_spawn(terminate_another_thread);
    if (result == -1) {
        std::cout << "Failed to create thread" << std::endl;
        exit(-1);
    }
    result = uthread_spawn(terminate_another_thread);
    if (result == -1) {
        std::cout << "Failed to create thread" << std::endl;
        exit(-1);
    }

    result = uthread_spawn(terminate_another_thread);
    if (result == -1) {
        std::cout << "Failed to create thread" << std::endl;
        exit(-1);
    }

    if (uthread_get_total_quantums() != 1) {
        std::cout << "Total quantums should be 1 after other thread creation, but got " << uthread_get_total_quantums() << std::endl;
        exit(-1);
    }

    terminate_another_thread();

    std::cout << "Done" << std::endl;
}
