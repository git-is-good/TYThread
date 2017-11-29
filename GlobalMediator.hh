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

#define globalMediator      GlobalMediator::Instance()
#define co_currentTask      globalMediator.getThisPerThreadMgr()->currentTask()
#define co_globalWaitMut    globalMediator.globalWaitMut_
#define co_globalWaitCond   globalMediator.globalWaitCond_
#define co_yield            co_currentTask->continuationOut()

struct ThreadLocalInfo : public NonCopyable {
    PerThreadMgr    pmgr;
    bool            isMainYield = false;
};

class GlobalMediator : public Singleton {
public:
    void addRunnable(TaskPtr ptr);
    bool run_once();
    void run();

    TaskPtr &currentTask() {
        return getThisPerThreadMgr()->currentTask__;
    }
    PerThreadMgr *getThisPerThreadMgr() {
        return &threadLocalInfos[thread_id]->pmgr;
    }

    /* called by the main thread to initailize all env */
    static void Init();
    static GlobalMediator &Instance() {
        static GlobalMediator g;
        return g;
    }
    static void TerminateGracefully() {
        Instance().terminatable = true;
    }
    static thread_local int thread_id;
    std::mutex globalWaitMut_;
    std::condition_variable globalWaitCond_;

private:
    bool terminatable = false;

    std::vector<std::unique_ptr<ThreadLocalInfo>> threadLocalInfos;
    std::vector<std::thread> children;
};

#endif /* _GLOBALMEDIATOR_HH_ */
