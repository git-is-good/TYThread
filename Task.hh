#ifndef _TASK_HH_
#define _TASK_HH_

#include "forward_decl.hh"
#include "util.hh"
#include "debug.hh"

#include <atomic>
#include <functional>
#include <memory>
#include <vector>
#include <boost/context/all.hpp>


#ifndef _UNIT_TEST_TASK_

#include "PerThreadMgr.hh"
#define co_currentTask globalTaskMgr.currentTask()

#else /* doing unit test */

extern TaskPtr bigTestTask;
#define co_currentTask bigTestTask

#endif /* _UNIT_TEST_TASK_ */

class Task 
    : public std::enable_shared_from_this<Task>,
      public NonCopyable
{
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

    explicit Task(std::function<void()> callback)
        : callback(callback)
        , debugId(++debugId_counter)
    {}
    ~Task();
    void addToGroup(TaskGroup *gp);
    void removeFromGroup(TaskGroup *gp);
    void terminate();
    void continuationIn();
    static void continuationOut();

//private:
    int state = Initial;
    std::function<void()>   callback;

    using continuation_t = boost::context::continuation;
    continuation_t          saved_continuation;
    continuation_t          task_continuation;

    std::vector<TaskGroup*> groups;

    int debugId;
    static std::atomic<int> debugId_counter;
};

#define co_yield Task::continuationOut()

#endif /* _TASK_HH_ */

