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
  bool isRealWait = false;
  {
      std::lock_guard<Spinlock> _(mut_);
      isRealWait = !taskPtrs.empty();
      if ( isRealWait ) {
          DEBUG_PRINT(DEBUG_TaskGroup, "task %d starts GroupWait at TaskGroup %d",
                  co_currentTask->debugId, debugId);
          co_currentTask->state = Task::GroupWait;
          co_currentTask->blockedBy = this;
      } else {
          DEBUG_PRINT(DEBUG_TaskGroup, "TaskGroup nothing to wait");
      }
  }

  if ( isRealWait ) {
      co_yield;
  }
}

bool
TaskGroup::resumeIfNothingToWait(TaskPtr &ptr)
{
    std::lock_guard<Spinlock> _(mut_);
    if ( taskPtrs.empty() ) {
        ptr->state = Task::Runnable;
        return true;
    } else {
        blockedTask = ptr;
        return false;
    }
}

void
TaskGroup::pushTaskPtr(TaskPtr &&ptr)
{
    std::lock_guard<Spinlock> _(mut_);
    taskPtrs.push_back(std::move(ptr));
}

TaskGroup&
TaskGroup::registe(TaskPtr &ptr)
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
    TaskPtr nowCanRun = nullptr;
    {
        std::lock_guard<Spinlock> _(mut_);

        DEBUG_PRINT(DEBUG_TaskGroup, "task %d informDone to TaskGroup %d...", ptr->debugId, debugId);
        auto iter = std::find(taskPtrs.begin(), taskPtrs.end(), ptr);
        MUST_TRUE(iter != taskPtrs.end(), "ptr: %d", ptr->debugId);
        taskPtrs.erase(iter);
        if ( taskPtrs.empty() ) {
            nowCanRun = std::move(blockedTask);
        }
    }

    if ( nowCanRun ) {
        nowCanRun->state = Task::Runnable;
        DEBUG_PRINT(DEBUG_TaskGroup,
                "informDone causes task %d blocked by %d runnable", nowCanRun->debugId, debugId);

        globalMediator.addRunnable(nowCanRun);
    }
}

/* If registe is called before, then *MUST* call wait,
 * because with raw pointer in Task, informDone might be called
 * when the destructor is executing */ 
TaskGroup::~TaskGroup()
{
    DEBUG_PRINT(DEBUG_TaskGroup, "TaskGroup %d destroying...", debugId);

    // defensive
    MUST_TRUE(blockedTask == nullptr,
            "TaskGroup %d blockedTask should be nullptr when TaskGroup is under destruction", debugId);

    MUST_TRUE(taskPtrs.empty(),
            "User error: TaskGroup %d destructor called without wait()", debugId);
}

std::atomic<int> TaskGroup::debugId_counter = {0};
