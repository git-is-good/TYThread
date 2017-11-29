#ifndef _PERTHREADMGR_HH_
#define _PERTHREADMGR_HH_

#include "forward_decl.hh"
#include "util.hh"
#include "Task.hh"
#include "Skiplist.hh"

#include <vector>
#include <memory>

class PerThreadMgr : public NonCopyable {
public:
    void addRunnable(TaskPtr &ptr) {
        runnable_queue->enqueue(ptr);
    }
    bool run_runnable();

    // stealing is done in GlobalMediator

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

    TaskPtr &currentTask() { return currentTask__; }
private:
    friend class GlobalMediator;
    std::unique_ptr<Skiplist<Task>> runnable_queue = std::make_unique<Skiplist<Task>>();

    std::vector<TaskPtr>    mpi_blocked_queue;

    TaskPtr                 currentTask__ = nullptr;
    int                     debugId;
};

#endif /* _PERTHREADMGR_HH_ */

