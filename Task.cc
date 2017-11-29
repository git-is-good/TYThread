/* 
 * 1) runnable_queue
 * 2) mpi_blocked_queue
 *
 * In every term of Run:
 *    start:
 *      -- if runnable_queue is not empty, pop one and run
 *      -- else traverse all threads to steal
 *         a runnable_queue task
 *              -- if success, run it
 *              -- else 
 *                      -- if mpi_blocked_queue is not empty
 *                          MPI_Test
 *                      -- else
 *                           mut_wait_task
 *                           cond_wait_task.wait()
 *
 *    when a task is spawned or restarted, cond_wait_task.notify_all()
 *
 *    // TaskHandle must be a shared_ptr to Task, because 
 *    // a Task might terminate before a group wait is fired
 *    using TaskHandle = std::shared_ptr<Task>;
 *
 *              // toy and toy_component must be already in a coroutine 
 *              void toy() {
 *                  TaskHandle id1 = go(foo, a, b);
 *                  TaskHandle id2 = go(bar, c, d);
 *                  TaskHandle id3 = go(baz, c, d);
 *
 *                  TaskGroup gp;
 *                  // a potential race-condition: if a raw pointer was used, then 
 *                  // no way to determine whether id1 is still valid at this point
 *                  gp.registe(id1).registe(id2);
 *
 *                  go(toy_component, id1, id3);
 *
 *                  gp.wait();
 *              }
 *
 *              void toy_component(TaskHandle id1, TaskHandle id3) {
 *                  TaskGroup gp;
 *                  gp.registe(id1).registe(id3);
 *
 *                  // do many things 
 *                  gp.wait();
 *
 *                  // do many things encore
 *              }
 *
 *
 *              // when encounter a TaskGroup::wait(),
 *              // task become a GroupWait state;
 *              // when a task terminates, it removes itself from 
 *              // relative groups
 *              
 *
 *  How to yield ?
 *  running code can be in 2 states:
 *  1) main-code
 *  2) coroutine
 *
 *  main-code scheduler can be in 2 states:
 *  1) normal
 *  2) main-yield: when some main-code yield itself, and the scheduler
 *      took the control flow
 *
 *  when co_yield is called:
 *  -- if in main-code:
 *          call main-code scheduler, and set the scheduler to main-yield state
 *          pop a Task from runnable_queue, or steal a Task
 *          -- if 1, has a poped Task and 2, has enough stack space and
 *             3, the poped Task is a pure computation:
 *             run it directly in the current stack
 *          -- else, regard the yielding function as in MPIBlocked,
 *             perform mpi_blocked_queue polling.
 *
 *  -- else, it's in a coroutine
 *          pop a Task from runnable_queue, or steal a Task
 *          -- if 1, has a poped Task and 2, has enough stack space and
 *             3, the poped Task is a pure computation:
 *             run it directly in the current stack
 *          -- else, continuationOut to the main-code scheduler
 *
 */

#include "GlobalMediator.hh"
#include "Task.hh"
#include "debug.hh"
#include "TaskGroup.hh"
#include "ObjectPool.hh"
#include "Config.hh"

#include <mutex>
#include <array>
#include <vector>
#include <algorithm>
#include <boost/context/all.hpp>

std::atomic<int> Task::debugId_counter = {0};

Task::~Task()
{
    DEBUG_PRINT(DEBUG_Task,
            "Task %d destructor called", debugId); 
}

void
Task::continuationOut()
{
    MUST_TRUE(saved_continuation, "task: %d", debugId);
    DEBUG_PRINT(DEBUG_Task, "Thread %d: Task %d continuationOut", 
            globalMediator.thread_id, debugId);
    saved_continuation = saved_continuation.resume();
}

bool
Task::addToGroup(TaskGroup *gp)
{
    std::lock_guard<Spinlock> _(mut_);

    DEBUG_PRINT(DEBUG_Task, "Task %d addToGroup %d...", debugId, gp->debugId);
    if ( !isFini() ) {
        groups.push_back(gp);
        gp->pushTaskPtr(TaskPtr(this));
        return true;
    } else {
        return false;
    }
}

void
Task::terminate()
{
    std::lock_guard<Spinlock> _(mut_);
    DEBUG_PRINT(DEBUG_Task, "Task %d terminating...", debugId);
    state = Terminated;

    for ( auto group : groups ) {
        group->informDone(TaskPtr(this));
    }
}

const char*
Task::getStateName(int s)
{
    static std::array<const char *, END_OF_STATE> dict = {
        "Initial",
        "Runnable",
        "MPIBlocked",
        "GroupWait",
        "Terminated",
    };
    if ( s < dict.size() ) {
        return dict[s];
    } else {
        return "<unknown>";
    }
}

void
Task::continuationIn()
{
    DEBUG_PRINT(DEBUG_Task, "Thread %d: Task %d continuationIn with state %s...",
            globalMediator.thread_id, debugId, getStateName(state));
    if ( state == Task::Runnable ) {
        MUST_TRUE(task_continuation, "task: %d", co_currentTask->debugId);
        task_continuation = task_continuation.resume();
    } else if ( state == Task::Initial ) {
        state = Task::Runnable;
        task_continuation = boost::context::callcc(
            [this] (continuation_t &&cont) -> continuation_t {
                saved_continuation = std::move(cont);
                try {
                    callback();
                    terminate();
                } catch ( std::exception const &e ) {
                    // TODO: add exception handling
                    terminate();
                }
                return std::move(saved_continuation);
            });
    }
}

void
Task::runInStack()
{
    DEBUG_PRINT(DEBUG_Task, "Thread %d: Task %d runInStack()", globalMediator.thread_id, debugId);
    MUST_TRUE(isPure, "runInStack() must be pure task");

    try {
        callback();
        terminate();
    } catch ( std::exception const &e ) {
        // TODO: add exception handling
        terminate();
    }
}

void
TaskPool::Init()
{
    ObjectPoolConfig config;
    config
        .set_num_of_thread(Config::Instance().num_of_threads)
        .set_pool_init_size(Config::Instance().init_task_pool_size)
        .set_enlarge_rate(Config::Instance().enlarge_rate)
        .set_max_total_cached(Config::Instance().max_total_cached)
        .set_cache_reclaim_period(Config::Instance().cache_reclaim_period)
        ;
    mediator = std::make_unique<ObjectPoolMediator<Task>>(config);
}

std::unique_ptr<ObjectPoolMediator<Task>> TaskPool::mediator;

void*
Task::operator new(std::size_t sz)
{
//    return ::operator new(sz);
    MUST_TRUE(sz == sizeof(Task), "Task new operator only for Task object");
    return TaskPool::Instance()->my_alloc(globalMediator.thread_id);
}

void
Task::operator delete(void *ptr, std::size_t)
{
//    ::operator delete (ptr);
    TaskPool::Instance()->my_release(globalMediator.thread_id, ptr);
}
