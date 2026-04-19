#include <iostream>
#include <uthreads.h>
#include <set>

int uthread_init(int quantum_usecs){
    return 0;
}

int uthread_get_tid(){
    return 0;
}

int uthread_spawn(thread_entry_point entry_point) {
    return -1;
}

int uthread_terminate(int tid) {
    return -1;
}
class ThreadIdManager {
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


        int removeThread(int id){
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

    private: 
        std::set <int> terminatedThreads;
        int currentMaxId; 
};