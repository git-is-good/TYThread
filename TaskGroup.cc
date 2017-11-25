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

TaskGroup::TaskGroup()
    : inCoroutine(globalMediator.inCoroutine())
{}

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
          co_currentTask->state = Task::GroupWait;
          co_currentTask->blockedBy = this;
      } else {
          DEBUG_PRINT(DEBUG_TaskGroup, "TaskGroup nothing to wait");
      }
  }

  if ( isRealWait ) {
      co_yield;
  }

//    bool isRealWait = false;
//    {
//        std::lock_guard<std::mutex> _(mut_);
//        isRealWait = !taskPtrs.empty();
//        blocked = isRealWait;
//    }
//
//    if ( isRealWait ) {
//        DEBUG_PRINT(DEBUG_TaskGroup, "task %d starts GroupWait at TaskGroup{%s} %d",
//                co_currentTask->debugId, inCoroutine ? "co" : "main-code", debugId);
//        if ( inCoroutine ) {
//            co_currentTask->state = Task::GroupWait;
//            co_currentTask->blockedBy = this;
//            co_yield;
//        } else {
//            // in main-code
//            for ( ;; ) {
//                if ( !blocked ) break;
//
//                if ( globalMediator.run_once() ) continue;
//
//                // nothing remaining ...
//                std::unique_lock<std::mutex> lock(co_globalWaitMut);
//                co_globalWaitCond.wait_for(lock, std::chrono::milliseconds(Config::Instance().max_wait_task_time));
//            }
//        }
//    } else {
//        DEBUG_PRINT(DEBUG_TaskGroup, "TaskGroup %d nothing to wait", debugId);
//    }
}

bool
TaskGroup::resumeIfNothingToWait(TaskPtr &ptr)
{
    std::lock_guard<std::mutex> _(mut_);
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
    std::lock_guard<std::mutex> _(mut_);
//    taskPtrs.push_back(std::move(ptr));
    taskPtrs.insert(std::move(ptr));
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
//    bool canrun = false;
    TaskPtr nowCanRun = nullptr;
    {
        std::lock_guard<std::mutex> _(mut_);

        DEBUG_PRINT(DEBUG_TaskGroup, "task %d informDone to TaskGroup %d...", ptr->debugId, debugId);
//        auto iter = std::find(taskPtrs.begin(), taskPtrs.end(), ptr);
        auto iter = taskPtrs.find(ptr);
        MUST_TRUE(iter != taskPtrs.end(), "ptr: %d", ptr->debugId);
        taskPtrs.erase(iter);
//        if ( blocked && taskPtrs.empty() ) {
        if ( taskPtrs.empty() ) {
            nowCanRun = std::move(blockedTask);
//            canrun = true;
//            blocked = false;
        }
    }

//    if ( canrun ) {
//        DEBUG_PRINT(DEBUG_TaskGroup,
//                "informDone causes task{%s} %d blocked by %d runnable", inCoroutine ? "co" : "main-code", blockedTask->debugId, debugId);
//        if ( inCoroutine ) {
//            blockedTask->state = Task::Runnable;
//            globalMediator.addRunnable(blockedTask);
//            blockedTask = nullptr;
//        } else {
//            co_globalWaitCond.notify_all();
//        }
//    }

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
