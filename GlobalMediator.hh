#ifndef _GLOBALMEDIATOR_HH_
#define _GLOBALMEDIATOR_HH_

#include "util.hh"
#include "forward_decl.hh"
#include "PerThreadMgr.hh"

#include <utility>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <thread>

class GlobalMediator : public Singleton {
public:
    void addRunnable(TaskPtr ptr);
    void run();

    TaskPtr currentTask() {
        return pmgrs[thread_id]->currentTask__;
    }

    static void TerminateGracefully() {
        Instance().terminatable = true;
    }


//private:
    static PerThreadMgr *getThisPerThreadMgr() {
        return Instance().pmgrs[thread_id].get();
    }
    static GlobalMediator &Instance() {
        static GlobalMediator g;
        return g;
    }
    static thread_local int thread_id;

    /* called by the main thread to initailize all env */
    static void Init();
    bool terminatable = false;

    std::vector<std::unique_ptr<PerThreadMgr>> pmgrs;

    std::mutex globalWaitMut_;
    std::condition_variable globalWaitCond_;

    std::vector<std::thread> children;
};

#define globalMediator      GlobalMediator::Instance()
#define co_currentTask      globalMediator.getThisPerThreadMgr()->currentTask__
#define co_globalWaitMut    globalMediator.globalWaitMut_
#define co_globalWaitCond   globalMediator.globalWaitCond_

#endif /* _GLOBALMEDIATOR_HH_ */
