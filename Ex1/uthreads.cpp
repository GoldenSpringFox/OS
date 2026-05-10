#include "uthreads.h"

#include <iostream>
#include <map>
#include <list>
#include <set>
#include <signal.h>
#include <memory>
#include <setjmp.h>
#include <sys/time.h>

#define USEC_IN_SEC 1000000

/**************************************************
*                                                 *
*                  Thread Setup                   *
*                                                 *
***************************************************/

#ifdef __x86_64__
/* code for 64 bit Intel arch */

typedef unsigned long address_t;
#define JB_SP 6
#define JB_PC 7

/* A translation is required when using an address of a variable.
   Use this as a black box in your code. */
address_t translate_address(address_t addr)
{
    address_t ret;
    asm volatile("xor    %%fs:0x30,%0\n"
        "rol    $0x11,%0\n"
                 : "=g" (ret)
                 : "0" (addr));
    return ret;
}

#else
/* code for 32 bit Intel arch */

typedef unsigned int address_t;
#define JB_SP 4
#define JB_PC 5


/* A translation is required when using an address of a variable.
   Use this as a black box in your code. */
address_t translate_address(address_t addr)
{
    address_t ret;
    asm volatile("xor    %%gs:0x18,%0\n"
                 "rol    $0x9,%0\n"
    : "=g" (ret)
    : "0" (addr));
    return ret;
}


#endif

typedef void (*thread_entry_point)(void);


/**************************************************
*                                                 *
*                   Classes                       *
*                                                 *
***************************************************/

class ThreadIdManager {
    private: 
        std::set<int> terminatedThreads{};
        int currentMaxId = 0;

    public:
        int getNewThreadId(){
            if (!terminatedThreads.empty()) {
                auto smallestIdPointer = terminatedThreads.begin();
                int smallestId = *(smallestIdPointer);
                terminatedThreads.erase(smallestId);
                return smallestId;
            }

            if (currentMaxId < MAX_THREAD_NUM - 1) {
                currentMaxId++;
                return currentMaxId;
            }
            
            std::cerr << "thread library error: passed max threads number\n";
            return -1;
    
        }

        int removeThreadId(int id){
            if (id == currentMaxId) {
                currentMaxId--;
                while (terminatedThreads.count(currentMaxId)>0) {
                    terminatedThreads.erase(currentMaxId);
                    currentMaxId--;
                }
                return 0;
            }

            if ((terminatedThreads.count(id) > 0) || (id < 0) || (id > currentMaxId)) {
                return -1;
            }

            terminatedThreads.insert(id);
            return 0;
        }

};

class Thread {        
    private:     
        int id;
        sigjmp_buf env;
        thread_entry_point entry_point;
        char* stack;
        int quantums;
        bool isBlocked;
        int remainingSleepQuantums;

    public:
        // Special constructor for the main thread, which doesn't have an entry point or a stack, and is already running when the library is initialized.
        Thread() {
            this->id = 0;
            this->entry_point = nullptr;
            this->stack = nullptr;
            sigsetjmp(env, 1);
            sigemptyset(&env->__saved_mask); 

            this->quantums = 1;
            this->isBlocked = false;
            this->remainingSleepQuantums = 0;
        }

        Thread(thread_entry_point entry_point, int id) {
            this->entry_point = entry_point;
            this->stack = new char[STACK_SIZE];
            this->id = id;
            if (this->id == -1) {
                throw std::runtime_error("thread library error: passed max threads number");
            }

            setup_thread(this->id, this->stack, this->entry_point);

            this->quantums = 0;            
        }
        
        ~Thread() {
            if (this->stack != nullptr) {
                delete[] this->stack;
                this->stack = nullptr;
            }
        }

        void setup_thread(int tid, char *stack, thread_entry_point entry_point)
        {
            // initializes env[tid] to use the right stack, and to run from the function 'entry_point', when we'll use
            // siglongjmp to jump into the thread.
            address_t sp = (address_t) stack + STACK_SIZE - sizeof(address_t);
            address_t pc = (address_t) entry_point;
            sigsetjmp(env, 1);
            (env->__jmpbuf)[JB_SP] = translate_address(sp);
            (env->__jmpbuf)[JB_PC] = translate_address(pc);
            sigemptyset(&env->__saved_mask);
        }

        int getId() const {
            return id;
        }

        sigjmp_buf& getContext() {
            return env;
        }

        thread_entry_point getEntryPoint() const {
            return entry_point;
        }

        int getQuantums() const {
            return quantums;
        }
        void incrementQuantums() {
            quantums++;
        }
        bool getIsBlocked() const {
            return isBlocked;
        }
        void setBlocked(bool blocked) {
            isBlocked = blocked;
        }
        int getRemainingSleepQuantums() const {
            return remainingSleepQuantums;
        }
        void setRemainingSleepQuantums(int num_quantums) {
            remainingSleepQuantums = num_quantums;
        }
        void decrementRemainingSleepQuantums() {
            if (remainingSleepQuantums > 0) {
                remainingSleepQuantums--;
            }
        }
};


/**************************************************
*                                                 *
*                  Variables                      *
*                                                 *
***************************************************/

int runningThread;
std::list<int> readyThreads;
std::set<int> blockedThreads;
std::map<int, std::unique_ptr<Thread>> threads;
ThreadIdManager idManager = ThreadIdManager();
int totalQuantums;
struct itimerval timer;


/**************************************************
*                                                 *
*                Private Methods                  *
*                                                 *
***************************************************/
void resetQuantumTimer();

// Helper function to block a specific signal (used for critical sections)
void block_signal(int sig) {
    sigset_t set; 
    sigemptyset(&set); 
    sigaddset(&set, sig); 
    sigprocmask(SIG_BLOCK, &set, nullptr); 
}

// Helper function to unblock a specific signal (used for critical sections)
void unblock_signal(int sig) {
    sigset_t set; 
    sigemptyset(&set); 
    sigaddset(&set, sig); 
    sigprocmask(SIG_UNBLOCK, &set, nullptr); 
}

int increment_quantum() {
    block_signal(SIGVTALRM);
    totalQuantums++;
    threads[runningThread]->incrementQuantums();

    for (auto it = blockedThreads.begin(); it != blockedThreads.end(); ) {
        int tid = *it;
        threads[tid]->decrementRemainingSleepQuantums();

        if (threads[tid]->getRemainingSleepQuantums() == 0 && !threads[tid]->getIsBlocked()) {
            readyThreads.push_back(tid);
            // erase returns the iterator to the next element
            it = blockedThreads.erase(it); 
        } else {
            // Only increment if we didn't erase
            ++it; 
        }
    }
    resetQuantumTimer();

    unblock_signal(SIGVTALRM);
    return 0;
}

int context_switch () {
    block_signal(SIGVTALRM);
    if (threads.find(runningThread) != threads.end()) {
        int ret_val = sigsetjmp(threads[runningThread]->getContext(), 1);
        if (ret_val != 0) {
            unblock_signal(SIGVTALRM);
            return 0;
        }
    }
    
    int tid = readyThreads.front();
    readyThreads.pop_front();
    runningThread = tid;

    if (threads.find(tid) == threads.end()) {
        std::cerr << "thread libary error: ready thread with id " << tid << " does not exist\n";
        unblock_signal(SIGVTALRM);
        return -1;
    }

    increment_quantum();
    siglongjmp(threads[runningThread]->getContext(), 1);
    
    std::cerr << "thread libary error: reached unreachable code" << std::endl;
    unblock_signal(SIGVTALRM);
    return 0;
}

void timer_handler_quantum(int sig) {
    readyThreads.push_back(runningThread);
    context_switch();
}

void resetQuantumTimer() {
    int result = setitimer(ITIMER_VIRTUAL, &timer, NULL);
    if (result)
    {
        printf("system error: setitimer failed");
    }
}

void initializeQuantumTimer(int quantum_usecs) {
    struct sigaction sa = {0};

    sa.sa_handler = &timer_handler_quantum;
    if (sigaction(SIGVTALRM, &sa, NULL) < 0)
    {
        printf("sigaction error.");
    }

    timer.it_value.tv_sec = quantum_usecs / USEC_IN_SEC;
    timer.it_value.tv_usec = quantum_usecs % USEC_IN_SEC;
    timer.it_interval.tv_sec = quantum_usecs / USEC_IN_SEC;
    timer.it_interval.tv_usec = quantum_usecs % USEC_IN_SEC;

    resetQuantumTimer();
}


/**************************************************
*                                                 *
*                 Public Methods                  *
*                                                 *
***************************************************/

/**
 * @brief initializes the thread library.
 *
 * Once this function returns, the main thread (tid == 0) will be set as RUNNING. There is no need to 
 * provide an entry_point or to create a stack for the main thread - it will be using the "regular" stack and PC.
 * You may assume that this function is called before any other thread library function, and that it is called
 * exactly once.
 * The input to the function is the length of a quantum in micro-seconds.
 * It is an error to call this function with non-positive quantum_usecs.
 *
 * @return On success, return 0. On failure, return -1.
*/
int uthread_init(int quantum_usecs) {
    if (quantum_usecs <= 0) {
        std::cerr << "thread library error: quantum_usecs must be positive\n";
        return -1;
    }

    threads[0] = std::make_unique<Thread>();
    runningThread = 0;

    totalQuantums = 1;

    initializeQuantumTimer(quantum_usecs);

    return 0;
}

/**
 * @brief Creates a new thread, whose entry point is the function entry_point with the signature
 * void entry_point(void).
 *
 * The thread is added to the end of the READY threads list.
 * The uthread_spawn function should fail if it would cause the number of concurrent threads to exceed the
 * limit (MAX_THREAD_NUM).
 * Each thread should be allocated with a stack of size STACK_SIZE bytes.
 * It is an error to call this function with a null entry_point.
 *
 * @return On success, return the ID of the created thread. On failure, return -1.
*/
int uthread_spawn(thread_entry_point entry_point) { 
    if (entry_point == nullptr) {
        std::cerr << "thread library error: entry point is null\n";
        return -1;
    }
    block_signal(SIGVTALRM);
    if (threads.size() >= MAX_THREAD_NUM) {
        std::cerr << "thread library error: passed max threads number\n";
        unblock_signal(SIGVTALRM);
        return -1;
    }

    int tid = idManager.getNewThreadId();
    std::unique_ptr<Thread> threadPtr;

    try {
        threadPtr = std::make_unique<Thread>(entry_point, tid);
    }
    catch (const std::runtime_error& e) { 
        std::cerr << "system error: " << e.what() << std::endl;
        exit(1);
    }
    threads.insert({threadPtr->getId(), std::move(threadPtr)});
    readyThreads.push_back(tid);
    unblock_signal(SIGVTALRM);
    return tid;
}


/**
 * @brief Terminates the thread with ID tid and deletes it from all relevant control structures.
 *
 * All the resources allocated by the library for this thread should be released. If no thread with ID tid exists it
 * is considered an error. Terminating the main thread (tid == 0) will result in the termination of the entire
 * process using exit(0) (after releasing the assigned library memory).
 *
 * @return The function returns 0 if the thread was successfully terminated and -1 otherwise. If a thread terminates
 * itself or the main thread is terminated, the function does not return.
*/
int uthread_terminate(int tid){
    block_signal(SIGVTALRM);
    if (tid == 0) {
        threads.clear();
        exit(0);
    }

    if (threads.find(tid) == threads.end()) {
        std::cerr << "thread library error: thread with id " << tid << " does not exist\n";
        unblock_signal(SIGVTALRM);
        return -1;
    }

    threads.erase(tid);
    readyThreads.remove(tid);
    blockedThreads.erase(tid);
    idManager.removeThreadId(tid);

    if (tid == runningThread) {
        context_switch();
    }
    unblock_signal(SIGVTALRM);
    return 0;
}


/**
 * @brief Blocks the thread with ID tid. The thread may be resumed later using uthread_resume.
 *
 * If no thread with ID tid exists it is considered as an error. In addition, it is an error to try blocking the
 * main thread (tid == 0). If a thread blocks itself, a scheduling decision should be made. Blocking a thread in
 * BLOCKED state has no effect and is *not* considered an error.
 *
 * @return On success, return 0. On failure, return -1.
*/
int uthread_block(int tid) {
    block_signal(SIGVTALRM);
    if (threads.find(tid) == threads.end()) {
        std::cerr << "thread library error: thread with id " << tid << " does not exist\n";
        unblock_signal(SIGVTALRM);
        return -1;
    }
    if (tid == 0) {
        std::cerr << "thread library error: cannot block main thread\n";
        unblock_signal(SIGVTALRM);  
        return -1;
    }
    if (runningThread == tid) {
        blockedThreads.insert(tid);
        context_switch();
        unblock_signal(SIGVTALRM);
        return 0;
    }
    if (blockedThreads.find(tid) != blockedThreads.end()) {
        unblock_signal(SIGVTALRM);  
        return 0;
    }
    threads[tid]->setBlocked(true);
    blockedThreads.insert(tid);
    unblock_signal(SIGVTALRM);
    return 0;
}


/**
 * @brief Resumes a blocked thread with ID tid and moves it to the READY state.
 *
 * Resuming a thread in a RUNNING or READY state has no effect and is not considered as an error. If no thread with
 * ID tid exists it is considered an error.
 * When a thread transition to the READY state it is placed at the end of the READY queue.
 *
 * @return On success, return 0. On failure, return -1.
*/
int uthread_resume(int tid) {
    block_signal(SIGVTALRM);
    if (runningThread == tid || readyThreads.front() == tid) {
        unblock_signal(SIGVTALRM);
        return 0;
    }
    if (threads.find(tid) == threads.end()) {
        std::cerr << "thread library error: thread with id " << tid << " does not exist\n";
        unblock_signal(SIGVTALRM);
        return -1;
    }
    threads[tid]->setBlocked(false);
    blockedThreads.erase(tid);
    readyThreads.push_back(tid);
    unblock_signal(SIGVTALRM);
    return 0;
}


/**
 * @brief Blocks the RUNNING thread for num_quantums quantums.
 *
 * Immediately after the RUNNING thread transitions to the BLOCKED state a scheduling decision should be made.
 * After the sleeping time is over, the thread should go back to the end of the READY queue.
 * If the thread which was just RUNNING should also be added to the READY queue, or if multiple threads wake up 
 * at the same time, the order in which they're added to the end of the READY queue doesn't matter.
 * The number of quantums refers to the number of times a new quantum starts, regardless of the reason. Specifically,
 * the quantum of the thread which has made the call to uthread_sleep isn’t counted.
 * A call with num_quantums == 0 will immediately stop the thread and move it to the back of the execution queue.
 * 
 * It is considered an error if the main thread (tid == 0) calls this function with num_quantums != 0.
 *
 * @return On success, return 0. On failure, return -1.
*/
int uthread_sleep(int num_quantums) {
    block_signal(SIGVTALRM);
    if (num_quantums < 0) {
        std::cerr << "thread library error: num_quantums must be non-negative\n";
        unblock_signal(SIGVTALRM);
        return -1;
    }

    if (num_quantums == 0) {
        readyThreads.push_back(runningThread);
        context_switch();
        unblock_signal(SIGVTALRM);                  
        return 0;
    }

    if (runningThread == 0) {
        std::cerr << "thread library error: main thread cannot sleep\n";
        unblock_signal(SIGVTALRM);
        return -1;
    }

    threads[runningThread]->setRemainingSleepQuantums(num_quantums);
    blockedThreads.insert(runningThread);
    context_switch();
    unblock_signal(SIGVTALRM);
    return 0;
}

/**
 * @brief Returns the thread ID of the calling thread.
 *
 * @return The ID of the calling thread.
*/
int uthread_get_tid() {
    return runningThread;
}


/**
 * @brief Returns the total number of quantums since the library was initialized, including the current quantum.
 *
 * Right after the call to uthread_init, the value should be 1.
 * Each time a new quantum starts, regardless of the reason, this number should be increased by 1.
 *
 * @return The total number of quantums.
*/
int uthread_get_total_quantums() {
    return totalQuantums;
}


/**
 * @brief Returns the number of quantums the thread with ID tid was in RUNNING state.
 *
 * On the first time a thread runs, the function should return 1. Every additional quantum that the thread starts should
 * increase this value by 1 (so if the thread with ID tid is in RUNNING state when this function is called, include
 * also the current quantum). If no thread with ID tid exists it is considered an error.
 *
 * @return On success, return the number of quantums of the thread with ID tid. On failure, return -1.
*/
int uthread_get_quantums(int tid) {
    block_signal(SIGVTALRM);
    if (threads.find(tid) == threads.end()) {
        std::cerr << "thread library error: thread with ID " << tid << " does not exist\n";
        unblock_signal(SIGVTALRM);
        return -1;
    }
    int quantums = threads[tid]->getQuantums();
    unblock_signal(SIGVTALRM);
    return quantums;
}
