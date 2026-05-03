#include "uthreads.h"

#include <iostream>
#include <map>
#include <list>
#include <set>
#include <signal.h>
#include <memory>
#include <setjmp.h>

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

            if (currentMaxId < MAX_THREAD_NUM) {
                currentMaxId++;
                return currentMaxId;
            }
            
            std::cerr << "ERROR: passed max threads number\n";
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
    public: 
        int id;
    private:     
        thread_entry_point entry_point;
        char* stack;
        sigjmp_buf env;

    public:
        // Special constructor for the main thread, which doesn't have an entry point or a stack, and is already running when the library is initialized.
        Thread() {
            this->id = 0;
            this->entry_point = nullptr;
            this->stack = nullptr;
            sigsetjmp(env, 1);
            sigemptyset(&env->__saved_mask);
        }

        Thread(thread_entry_point entry_point, int id) {
            this->entry_point = entry_point;
            this->stack = new char[STACK_SIZE];
            this->id = id;
            if (this->id == -1) {
                throw std::runtime_error("ERROR: passed max threads number");
            }

            setup_thread(this->id, this->stack, this->entry_point);
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


/**************************************************
*                                                 *
*                   Methods                       *
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
        std::cerr << "ERROR: quantum_usecs must be positive\n";
        return -1;
    }

    threads[0] = std::make_unique<Thread>();
    runningThread = 0;

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
    // block_signal(SIGVTALRM); 
    if (entry_point == nullptr) {
        std::cerr << "ERROR: entry point is null\n";
        //unblock_signal(SIGVTALRM);
        return -1;
    }
    if (threads.size() >= MAX_THREAD_NUM - 1) {
        std::cerr << "ERROR: passed max threads number\n";
        //unblock_signal(SIGVTALRM);
        return -1;
    }

    int tid = idManager.getNewThreadId();
    std::unique_ptr<Thread> threadPtr = std::make_unique<Thread>(entry_point, tid);
    threads.insert({threadPtr->id, std::move(threadPtr)});
    readyThreads.push_back(tid);
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
    if (tid == 0) {
        threads.clear();
        exit(0);
    }

    if (threads.find(tid) == threads.end()) {
        std::cerr << "ERROR: thread with id " << tid << " does not exist\n";
        return -1;
    }

    threads.erase(tid);
    readyThreads.remove(tid);
    blockedThreads.erase(tid);
    idManager.removeThreadId(tid);
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
    std::cerr << "thread library error: " << "did not implement" << std::endl;
    return -1;
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
    std::cerr << "thread library error: " << "did not implement" << std::endl;
    return -1;
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
    std::cerr << "thread library error: " << "did not implement" << std::endl;
    return -1;
}


/**
 * @brief Returns the thread ID of the calling thread.
 *
 * @return The ID of the calling thread.
*/
int uthread_get_tid() {
    return 0;
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
    std::cerr << "thread library error: " << "did not implement" << std::endl;
    return -1;
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
    std::cerr << "thread library error: " << "did not implement" << std::endl;
    return -1;
}

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
