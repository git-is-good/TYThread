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
#define co_currentTask      globalMediator.getThisPerThreadMgr()->currentTask__
#define co_globalWaitMut    globalMediator.globalWaitMut_
#define co_globalWaitCond   globalMediator.globalWaitCond_
#define co_yield            globalMediator.yieldRequest()

struct ThreadLocalInfo : public NonCopyable {
    PerThreadMgr    pmgr;
    bool            isMainYield = false;
};

class GlobalMediator : public Singleton {
public:
    bool inCoroutine();
    bool hasEnoughStack();
    void yieldRequest();
    void addRunnable(TaskPtr ptr);
    bool run_once();
    void run();

    TaskPtr currentTask() {
        return getThisPerThreadMgr()->currentTask__;
    }

    static void TerminateGracefully() {
        Instance().terminatable = true;
    }


//private:
    static PerThreadMgr *getThisPerThreadMgr() {
        return &Instance().threadLocalInfos[thread_id]->pmgr;
    }
    static GlobalMediator &Instance() {
        static GlobalMediator g;
        return g;
    }
    static thread_local int thread_id;

    /* called by the main thread to initailize all env */
    static void Init();
    bool terminatable = false;

    std::vector<std::unique_ptr<ThreadLocalInfo>> threadLocalInfos;

    std::mutex globalWaitMut_;
    std::condition_variable globalWaitCond_;

    std::vector<std::thread> children;
};

#endif /* _GLOBALMEDIATOR_HH_ */
