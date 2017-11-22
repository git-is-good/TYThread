#include "Task.hh"
#include "GlobalMediator.hh"
#include "PerThreadMgr.hh"
#include "debug.hh"
#include "Config.hh"

#include <chrono>

#ifdef _UNIT_TEST_PER_THREAD_MGR_
PerThreadMgr globalTaskMgr;
#endif

bool
PerThreadMgr::run_runnable()
{
    auto res_pair = runnable_queue.try_dequeue();
    if ( res_pair.first ) {
        // runnable found
        TaskPtr ptr = std::move(res_pair.second);
        DEBUG_PRINT(DEBUG_PerThreadMgr,
                "PerThreadMgr %d: runnable got locally: task %d...", debugId, ptr->debugId);

        MUST_TRUE(ptr != nullptr, "PerThreadMgr: %d", debugId);
        currentTask__ = ptr;
        DEBUG_PRINT(DEBUG_PerThreadMgr,
                "PerThreadMgr %d: task %d about to continuationIn @%p...", debugId, ptr->debugId, ptr.get());

        //TODO: race-condition: when continuationOut with
        // state GroupWait, another thread can wake it up
        // before this "switch" statement, which leads
        // this Task being enqueueed 2 times
        //Done
        ptr->continuationIn();

        MUST_TRUE(ptr->proccessedAfterYield == false,
                "task %d proccessedAfterYield corrupted", ptr->debugId);
        
        switch ( ptr->state ) {
        case Task::Runnable:
            runnable_queue.enqueue(ptr);
            break;
        case Task::MPIBlocked:
            mpi_blocked_queue.push_back(ptr);
            currentTask__ = nullptr;
            break;
        default:
            currentTask__ = nullptr;
        }
        ptr->proccessedAfterYield = true;
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
            runnable_queue.enqueue(ptr);
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
//    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    co_globalWaitCond.wait_for(lock, std::chrono::milliseconds(Config::Instance().max_wait_task_time));
}
