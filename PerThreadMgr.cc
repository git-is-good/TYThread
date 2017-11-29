#include "Task.hh"
#include "GlobalMediator.hh"
#include "PerThreadMgr.hh"
#include "TaskGroup.hh"
#include "debug.hh"
#include "Config.hh"

#include <mutex>
#include <chrono>

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

        ptr->continuationIn();
        currentTask__ = nullptr;

        switch ( ptr->state ) {
        case Task::Runnable:
            runnable_queue->enqueue(ptr);
            break;
        case Task::MPIBlocked:
            mpi_blocked_queue.push_back(ptr);
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
    for ( auto ptr : mpi_blocked_queue ) {
        ptr->continuationIn();
        if ( ptr->state == Task::Runnable ) {
            DEBUG_PRINT(DEBUG_PerThreadMgr,
                    "PerThreadMgr %d: MPIBlocked task %d runnable", debugId, ptr->debugId);
            runnable_queue->enqueue(ptr);
            currentTask__ = ptr;
            return true;
        }
    }
    DEBUG_PRINT(DEBUG_PerThreadMgr,
            "PerThreadMgr %d: either no MPIBlocked becomes runnable, either someone run and MPIBlocked again", debugId);
    return false;
}

#include <thread>

void
PerThreadMgr::wait_task()
{
    std::unique_lock<std::mutex> lock(co_globalWaitMut);
    DEBUG_PRINT(DEBUG_PerThreadMgr, "PerThreadMgr %d: starts sleeping...", debugId);
    co_globalWaitCond.wait_for(lock, std::chrono::milliseconds(Config::Instance().max_wait_task_time));
}
