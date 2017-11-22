#include "Config.hh"
#include "GlobalMediator.hh"
#include "TaskGroup.hh"
#include "Task.hh"
#include "debug.hh"

#include <algorithm>
#include <mutex>
#include <memory>
#include <utility>
#include <vector>
#include <thread>
#include <chrono>

// a TaskGroup can hold a Task, only if wait() is called,
// until the moment when wait() yields, proccessedAfterYield
// is always false; thus prevents other threads informDone it up
void
TaskGroup::wait()
{
    bool isRealWait = false;
    {
        std::lock_guard<std::mutex> _(mut_);
        isRealWait = !taskPtrs.empty();
        if ( isRealWait ) {
            DEBUG_PRINT(DEBUG_TaskGroup, "task %d starts GroupWait at TaskGroup %d",
                    co_currentTask->debugId, debugId);
            blockedTask = co_currentTask;
            co_currentTask->state = Task::GroupWait;
        } else {
            DEBUG_PRINT(DEBUG_TaskGroup, "TaskGroup nothing to wait");
        }
    }

    if ( isRealWait ) {
        co_yield;
    }
}

TaskGroup&
TaskGroup::registe(TaskPtr ptr)
{
    std::lock_guard<std::mutex> _(ptr->mut_);
    if ( ptr->isFini() ) return *this;

    // after this point, real register
    DEBUG_PRINT(DEBUG_TaskGroup, "task %d registering to TaskGroup %d...", ptr->debugId, debugId);
    ptr->addToGroup_locked(this);

    std::lock_guard<std::mutex> __(mut_);
    taskPtrs.push_back(ptr);
    return *this;
}

void
TaskGroup::informDone(TaskPtr ptr)
{
    TaskPtr nowCanRun = nullptr;
    {
        std::lock_guard<std::mutex> _(mut_);

        DEBUG_PRINT(DEBUG_TaskGroup, "task %d informDone to TaskGroup %d...", ptr->debugId, debugId);
        auto iter = std::find(taskPtrs.begin(), taskPtrs.end(), ptr);
        MUST_TRUE(iter != taskPtrs.end(), "ptr: %d", ptr->debugId);
        taskPtrs.erase(iter);
        if ( taskPtrs.empty() ) {
            nowCanRun = std::move(blockedTask);
        }
    }

    if ( nowCanRun ) {
        while ( !nowCanRun->proccessedAfterYield ) {
            DEBUG_PRINT(DEBUG_TaskGroup,
                    "rare but ok: task %d woken up at Thread %d from GroupWait before proccessedAfterYield",
                    nowCanRun->debugId, debugId);
            std::this_thread::sleep_for(
                    std::chrono::milliseconds(Config::Instance().continuation_ret_conflict_period));

        }
        nowCanRun->state = Task::Runnable;
        DEBUG_PRINT(DEBUG_TaskGroup,
                "informDone causes task %d blocked by %d runnable", nowCanRun->debugId, debugId);

#ifdef _UNIT_TEST_PER_THREAD_MGR_ 

        globalTaskMgr.addRunnable(nowCanRun);

#else /* _UNIT_TEST_PER_THREAD_MGR_ */

        globalMediator.addRunnable(nowCanRun);

#endif /* _UNIT_TEST_PER_THREAD_MGR_ */
    }
}

/* no need to use a lock, user is responsible to ensure
 * there is no access to this object when destructor is called */
TaskGroup::~TaskGroup()
{
    DEBUG_PRINT(DEBUG_TaskGroup, "TaskGroup %d destroying...", debugId);
    for ( auto & ptr : taskPtrs ) {
        ptr->removeFromGroup(this);
    }
}

std::atomic<int> TaskGroup::debugId_counter = {0};
