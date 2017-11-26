#include "GlobalMediator.hh"
#include "Task.hh"
#include "PerThreadMgr.hh"
#include "Config.hh"
#include "debug.hh"
#include "util.hh"
#include "Skiplist.hh"

#include <thread>
#include <mutex>
#include <condition_variable>
#include <algorithm>
#include <memory>
#include <utility>

void
GlobalMediator::Init()
{
    int num_of_threads = Config::Instance().num_of_threads;
    DEBUG_PRINT(DEBUG_GlobalMediator, 
            "Init() with num_of_threads = %d", 
            num_of_threads);
    Instance().threadLocalInfos.resize(num_of_threads);
    for ( auto &ptr : Instance().threadLocalInfos ) {
        ptr = std::make_unique<ThreadLocalInfo>();
    }
    
    /* main thread has thread_id 0 */
    thread_id = 0;
    Instance().threadLocalInfos[0]->pmgr.debugId = 0;
    for ( int i = 1; i < num_of_threads; ++i ) {
        std::thread tid(
            [i] () -> void {
                globalMediator.thread_id = i;
                globalMediator.getThisPerThreadMgr()->debugId = globalMediator.thread_id;
                globalMediator.run();
                DEBUG_PRINT(DEBUG_GlobalMediator,
                        "Thread %d: Terminating.", i);
            });
        globalMediator.children.push_back(std::move(tid));
    }
}

void
GlobalMediator::addRunnable(TaskPtr ptr)
{
    DEBUG_PRINT(DEBUG_GlobalMediator, "Thread %d: Mediator adds task %d at state %s runnable",
            thread_id, ptr->debugId, Task::getStateName(ptr->state));
    getThisPerThreadMgr()->addRunnable(ptr);
    co_globalWaitCond.notify_all();
}

void
GlobalMediator::yieldRequest()
{
    if ( inCoroutine() ) {
        TaskPtr &ptr = currentTask();
        DEBUG_PRINT(DEBUG_GlobalMediator, "Thread %d: Task %d inCoroutine yield with state %s",
                thread_id, ptr->debugId, Task::getStateName(ptr->state));
//        if ( ptr->state == Task::GroupWait ) {
//            // no choice, because PerThreadMgr needs to 
//            // handle it
//            ptr->continuationOut();
//            return;
//        }

        PerThreadMgr *mgr = getThisPerThreadMgr();

        DEBUG_PRINT(DEBUG_GlobalMediator, "Thread %d: Task %d runInStackPureTask() after yield inCoroutine",
                thread_id, ptr->debugId);
        //TODO: how to reduce enqueue/dequeue
//        while ( mgr->runInStackPureTask() )
//            ;

        ptr->continuationOut();
    } else {
        // in main-code
        run_once();
    }
}

bool
GlobalMediator::hasEnoughStack()
{
    return true;
}

bool
GlobalMediator::inCoroutine()
{
    return co_currentTask != nullptr;
}

bool
GlobalMediator::run_once()
{
    PerThreadMgr *mgr = getThisPerThreadMgr();
    //TODO: how to ....
//    if ( mgr->runInStackPureTask() ) return true;
    if ( mgr->run_runnable() ) return true;

    // no runnable, traverse and steal
    DEBUG_PRINT(DEBUG_GlobalMediator, "Thread %d: No runnable got locally, try steal", thread_id);
    bool stealSuccess = false;
    for ( int i = 0; i < Config::Instance().num_of_threads; i++ ) {
        if ( thread_id == i ) {
            continue;
        }

//        auto pair = threadLocalInfos[i]->pmgr.runnable_queue->try_dequeue_half();
        auto skiplistptr = threadLocalInfos[i]->pmgr.runnable_queue->dequeue_half();
        if ( !skiplistptr ) {
//        if ( !pair.first ) {
            // nothing there
            continue;
        }

        // steal succeeded
        DEBUG_PRINT(DEBUG_Special | DEBUG_GlobalMediator, "Thread %d: stole from %d %lu tasks",
                thread_id, i, skiplistptr->size());
//                thread_id, i, pair.second.size());
//        mgr->runnable_queue->enqueue_by_move(pair.second.begin(), pair.second.end());
        mgr->runnable_queue = std::move(skiplistptr);
        stealSuccess = true;
        break;
    }

    if ( stealSuccess ) return true;

    DEBUG_PRINT(DEBUG_GlobalMediator, "Thread %d: Nothing to steal...", thread_id);
    if ( !mgr->mpi_blocked_queue.empty() ) {
        DEBUG_PRINT(DEBUG_GlobalMediator, "Thread %d: polling mpi_blocked_queue", thread_id);
        mgr->run_mpi_blocked();
        return true;
    } 

    return false;
}

void
GlobalMediator::run()
{
    PerThreadMgr *mgr = getThisPerThreadMgr();
    for ( ;; ) {
        if ( run_once() ) continue;

        // no task, no mpi_blocked_queue
        // TODO: Do we need to try stealing mpi_blocked_queue from others ?
        if ( !terminatable ) {
            DEBUG_PRINT(DEBUG_GlobalMediator, "Thread %d: mpi_blocked_queue is empty, to sleep", thread_id);
            mgr->wait_task();
        } else if ( thread_id != 0 ) {
            // normal thread
            DEBUG_PRINT(DEBUG_GlobalMediator, "Thread %d: nothing to do and terminatable, terminate.", thread_id);
            break;
        } else {
            // main thread, needs to sweep all children
            for ( auto &thread : children ) {
                thread.join();
            }
            break;
        }
    }
}

thread_local int GlobalMediator::thread_id;

