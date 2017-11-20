#ifndef _GLOBALSCHEDULER_HH_
#define _GLOBALSCHEDULER_HH_

#include "util.hh"
#include "forward_decl.hh"
#include "PerThreadMgr.hh"

#include <utility>
#include <memory>
#include <atomic>

class GlobalScheduler : public Singleton {
public:
    void addToRunnableQueue(TaskPtr ptr);
    void run();

    TaskPtr currentTask() {
        return pmgrs[thread_id]->currentTask__;
    }

private:
    static PerThreadMgr *getThisPerThreadMgr() {
        return pmgrs[thread_id].get();
    }
    static GlobalScheduler &Instance() {
        static GlobalScheduler g;
        return g;
    }
    static thread_local int thread_id;
    static std::atomic<int> thread_id_counter;

    static void Init();

    int determineThreads();
    std::vector<std::unique_ptr<PerThreadMgr>> pmgrs;
};

#endif /* _GLOBALSCHEDULER_HH_ */

