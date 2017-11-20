#ifndef _PERTHREADMGR_HH_
#define _PERTHREADMGR_HH_

#include "forward_decl.hh"
#include "util.hh"
#include "Blockings.hh"

#include <vector>

class PerThreadMgr : public NonCopyable {
public:
    void addRunnable(TaskPtr ptr) {
        runnable_queue.enqueue(ptr);
    }
    bool run_runnable();

    // stealing is done in GlobalScheduler

    bool run_mpi_blocked();

    // wait at this condition with timeout
    void wait_task();

    void debug_run() {
        for (;;) {
            if ( run_runnable() ) {
                continue;
            }
            if ( run_mpi_blocked() ) {
                continue;
            }
            break;
            wait_task();
        }
    }

    /* for debug */
    TaskPtr currentTask() { return currentTask__; }
private:
    BlockingQueue<TaskPtr>  runnable_queue;

    std::vector<TaskPtr>    mpi_blocked_queue;

    TaskPtr                 currentTask__ = nullptr;
};

#ifdef _UNIT_TEST_PER_THREAD_MGR_
extern PerThreadMgr globalTaskMgr;
#endif /* _UNIT_TEST_PER_THREAD_MGR_ */

#endif /* _PERTHREADMGR_HH_ */

