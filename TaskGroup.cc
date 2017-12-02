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

void
TaskGroup::wait()
{
    if ( blocking_count != 0 ) {
        DEBUG_PRINT(DEBUG_TaskGroup, "task %d starts GroupWait at TaskGroup %d",
                co_currentTask->debugId, debugId);
        co_currentTask->state = Task::GroupWait;
        co_currentTask->blockedBy = this;
        co_yield;
    } else {
        DEBUG_PRINT(DEBUG_TaskGroup, "TaskGroup nothing to wait");
    }
}

bool
TaskGroup::resumeIfNothingToWait(TaskPtr &ptr)
{
    std::lock_guard<Spinlock> _(mut_);
    if ( blocking_count == 0 ) {
        ptr->state = Task::Runnable;
        return true;
    } else {
        blockedTask = ptr;
        return false;
    }
}

TaskGroup&
TaskGroup::registe(TaskPtr ptr)
{
    if ( !ptr->addToGroup(this) ) {
        return *this;
    }

    DEBUG_PRINT(DEBUG_TaskGroup, "task %d registering to TaskGroup %d...", ptr->debugId, debugId);
    return *this;
}

void
TaskGroup::informDone(TaskPtr ptr)
{
    std::lock_guard<Spinlock> _(mut_);
    TaskPtr nowCanRun = nullptr;

    if ( --blocking_count == 0 ) {
        DEBUG_PRINT(DEBUG_TaskGroup, "task %d informDone to TaskGroup %d...", ptr->debugId, debugId);
        nowCanRun = std::move(blockedTask);
    }

    if ( nowCanRun ) {
        nowCanRun->state = Task::Runnable;
        DEBUG_PRINT(DEBUG_TaskGroup,
                "informDone causes task %d blocked by %d runnable", nowCanRun->debugId, debugId);

        globalMediator.addRunnable(nowCanRun);
    }
}

TaskGroup::~TaskGroup()
{
    DEBUG_PRINT(DEBUG_TaskGroup, "TaskGroup %d destroying...", debugId);
}

std::atomic<int> TaskGroup::debugId_counter = {0};
