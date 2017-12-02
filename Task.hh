#ifndef _TASK_HH_
#define _TASK_HH_

#include "util.hh"
#include "debug.hh"
#include "Spinlock.hh"
#include "Skiplist.hh"
#include "ObjectPool.hh"

#include <atomic>
#include <mutex>
#include <functional>
#include <memory>
#include <vector>
#include <boost/context/all.hpp>

class TaskGroup;

class Task 
    : public RefCounted
    , public Linkable<Task>
{
    friend class PerThreadMgr;
    friend class TaskGroup;
    friend class GlobalMediator;
public:
    enum {
        /* when the Task is spawned */
        Initial,

        /* in the runnable_queue */
        Runnable,

        /* in the mpi_blocked_queue */
        MPIBlocked,

        /* not in any queue, in the on-stack TaskGroup */
        GroupWait,

        /* the task is about to be deleted,
         * but maybe it's in a TaskGroup,
         * so let the shared_ptr delete it automatically
         * when there is no ref count */
        Terminated,

        END_OF_STATE,
    };

    static const char *getStateName(int s);

    explicit Task(std::function<void()> callback, bool isPure = false)
        : callback(callback)
        , debugId(++debugId_counter)
    {
        DEBUG_PRINT(DEBUG_Task, "Task %d creating", debugId);
    
    }
    ~Task();
    bool addToGroup(TaskGroup *gp);
    void removeFromGroup(TaskGroup *gp);
    void terminate();

    void setPure(bool v = true) { isPure = v; }
    void runInStack();

    void continuationIn();
    void continuationOut();

    int debugId;
    int state = Initial;

    // memory management
    static void* operator new(std::size_t sz);
    static void operator delete(void *p, std::size_t sz);
private:
    std::function<void()>   callback;

    /* a pure task will not block, and can be scheduled in the current stack */
    bool                    isPure = false;

    bool isFini() const {
        return state == Task::Terminated;
    }

    using continuation_t = boost::context::continuation;
    continuation_t          saved_continuation;
    continuation_t          task_continuation;

    /* this vector will be accessed concurrently */
    TaskGroup               *blockedBy = nullptr;
    std::vector<TaskGroup*> groups;
    Spinlock                mut_;

    static std::atomic<int> debugId_counter;
};

using TaskPtr = DerivedRefPtr<Task>;

class TaskPool : public NonCopyable {
public:
    static void Init();

    static std::unique_ptr<ObjectPoolMediator<Task>> &Instance() {
        return mediator;
    }

    static void Terminate() {
        mediator->daemonTerminate();
    }
private:
    static std::unique_ptr<ObjectPoolMediator<Task>> mediator;
};

#endif /* _TASK_HH_ */

