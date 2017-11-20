#include "Task.hh"
#include "PerThreadMgr.hh"
#include "debug.hh"

PerThreadMgr globalTaskMgr;

bool
PerThreadMgr::run_runnable()
{
    auto res_pair = runnable_queue.try_dequeue();
    if ( res_pair.first ) {
        DEBUG_PRINT(DEBUG_PerThreadMgr,
                "runnable found: task %d...", res_pair.second->debugId);
        // runnable found
        currentTask__ = res_pair.second;
        res_pair.second->continuationIn();
        switch ( res_pair.second->state ) {
        case Task::Runnable:
            runnable_queue.enqueue(res_pair.second);
            break;
        case Task::MPIBlocked:
            mpi_blocked_queue.push_back(res_pair.second);
            currentTask__ = nullptr;
            break;
        default:
            currentTask__ = nullptr;
        }
        return true;
    } else {
        DEBUG_PRINT(DEBUG_PerThreadMgr, "no runnable found");
        return false;
    }
}

bool
PerThreadMgr::run_mpi_blocked()
{
    for ( auto ptr : mpi_blocked_queue ) {
        ptr->continuationIn();
        if ( ptr->state == Task::Runnable ) {
            DEBUG_PRINT(DEBUG_PerThreadMgr, "MPIBlocked task %d runnable", ptr->debugId);
            runnable_queue.enqueue(ptr);
            currentTask__ = ptr;
            return true;
        }
    }
    DEBUG_PRINT(DEBUG_PerThreadMgr, "either no MPIBlocked becomes runnable, either someone run and MPIBlocked again");
    return false;
}

void
PerThreadMgr::wait_task()
{
//    std::unique_lock lock(getGlobalWaitMut());
    DEBUG_PRINT(DEBUG_PerThreadMgr, "PerThreadMgr starts sleeping...");
//    getGlobalWaitCond().wait(lock, getConfig().wait_task_timeout);
}
