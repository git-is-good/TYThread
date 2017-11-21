#include "TaskGroup.hh"
#include "Task.hh"
#include "debug.hh"

#include <algorithm>
#include <mutex>
#include <memory>
#include <utility>
#include <vector>

void
TaskGroup::wait()
{
    bool isRealWait = false;
    {
        std::lock_guard<std::mutex> _(mut_);
        isRealWait = !taskPtrs.empty();
        if ( isRealWait ) {
            DEBUG_PRINT(DEBUG_TaskGroup, "task %d starts GroupWait", co_currentTask->debugId);
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
    std::lock_guard<std::mutex> _(mut_);

    DEBUG_PRINT(DEBUG_TaskGroup, "task registering to TaskGroup ...");
    taskPtrs.push_back(ptr);
    ptr->addToGroup(this);
    return *this;
}

void
TaskGroup::informDone(TaskPtr ptr)
{
    TaskPtr nowCanRun = nullptr;
    {
        std::lock_guard<std::mutex> _(mut_);

        DEBUG_PRINT(DEBUG_TaskGroup, "task informDone to TaskGroup ...");
        auto iter = std::find(taskPtrs.begin(), taskPtrs.end(), ptr);
        taskPtrs.erase(iter);
        if ( taskPtrs.empty() ) {
            nowCanRun = std::move(blockedTask);
        }
    }

    if ( nowCanRun ) {
        nowCanRun->state = Task::Runnable;
        DEBUG_PRINT(DEBUG_TaskGroup, "informDone causes task %d runnable", nowCanRun->debugId);
        globalTaskMgr.addRunnable(nowCanRun);
    }
}

TaskGroup::~TaskGroup()
{
    std::lock_guard<std::mutex> _(mut_);

    DEBUG_PRINT(DEBUG_TaskGroup, "TaskGroup destroying...");
    for ( auto & ptr : taskPtrs ) {
        ptr->removeFromGroup(this);
    }
}

