#include "Task.hh"
#include "GlobalMediator.hh"
#include "PerThreadMgr.hh"
#include "TaskGroup.hh"
#include "debug.hh"
#include "Config.hh"

#include <mutex>
#include <chrono>
#include <vector>
#include <algorithm>

void
PerThreadMgr::handle_after_continuationOut(TaskPtr &ptr)
{
    switch ( ptr->state ) {
    case Task::Runnable:
        runnable_queue->enqueue(ptr);
        break;
    case Task::MPIBlocked:
        mpi_blocked_queue.push_back(ptr);
        DEBUG_PRINT(DEBUG_PerThreadMgr,
                "PerThreadMgr %d: MPIBlocked got Task %d", debugId, ptr->debugId);
        break;
    case Task::GroupWait:
        DEBUG_PRINT(DEBUG_TaskGroup,
                "Task %d continuationOut with GroupWait at TaskGroup %d",
                ptr->debugId, ptr->blockedBy->debugId);

        if ( ptr->blockedBy->resumeIfNothingToWait(ptr) ) {
            runnable_queue->enqueue(ptr);
        }
        break;
    default:
        ;
    }
}

bool
PerThreadMgr::run_runnable()
{
    TaskPtr ptr = runnable_queue->dequeue();
    if ( ptr ) {
        // runnable found
        DEBUG_PRINT(DEBUG_PerThreadMgr,
                "PerThreadMgr %d: runnable got locally: task %d...", debugId, ptr->debugId);

        MUST_TRUE(ptr != nullptr, "PerThreadMgr: %d", debugId);
        currentTask__ = ptr;

        if ( ptr->isPure ) {
            ptr->runInStack();
        } else {
            ptr->continuationIn();
        }
        currentTask__ = nullptr;
        handle_after_continuationOut(ptr);

        return true;
    } else {
        DEBUG_PRINT(DEBUG_PerThreadMgr,
                "PerThreadMgr %d: no runnable got locally", debugId);
        return false;
    }
}

bool
PerThreadMgr::run_mpi_blocked()
{
    for ( auto &ptr : mpi_blocked_queue ) {
        currentTask__ = ptr;
        ptr->continuationIn();
        currentTask__ = nullptr;

        if ( ptr->state != Task::MPIBlocked ) {
            handle_after_continuationOut(ptr);
            mpi_blocked_queue.erase(std::find(mpi_blocked_queue.begin(), mpi_blocked_queue.end(), ptr));
            return true;
        } else {
            /* still MPIBlocked, note that it might run for a while
             * and fall into MPIBlocked state once again
             */
        }
    }
    DEBUG_PRINT(DEBUG_PerThreadMgr,
            "PerThreadMgr %d: either no MPIBlocked becomes runnable, either someone run and MPIBlocked again", debugId);
    return false;
}

void
PerThreadMgr::wait_task()
{
    std::unique_lock<std::mutex> lock(co_globalWaitMut);
    DEBUG_PRINT(DEBUG_PerThreadMgr, "PerThreadMgr %d: starts sleeping...", debugId);
    ++globalMediator.sleep_count;
    co_globalWaitCond.wait_for(lock, std::chrono::microseconds(Config::Instance().max_wait_task_time));
    --globalMediator.sleep_count;
}
