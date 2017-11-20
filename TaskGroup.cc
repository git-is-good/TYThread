//#include "GlobalScheduler.hh"
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
    std::lock_guard<std::mutex> _(mut_);

    DEBUG_PRINT(DEBUG_TaskGroup, "task informDone to TaskGroup ...");
    auto iter = std::find(taskPtrs.begin(), taskPtrs.end(), ptr);
    taskPtrs.erase(iter);
    if ( taskPtrs.empty() ) {
        DEBUG_PRINT(DEBUG_TaskGroup, "this task informDone causes taskPtrs empty() in TaskGroup ...");
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

