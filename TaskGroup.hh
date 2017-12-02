#ifndef _TASKGROUP_HH_
#define _TASKGROUP_HH_

#include "util.hh"
#include "Task.hh"

#include <memory>
#include <vector>
#include <mutex>
#include <atomic>

/* TaskGroup might be accessed by multiple threads
 * through informDone();
 */
class TaskGroup : public NonCopyable {
public:
    void wait();
    TaskGroup &registe(TaskPtr ptr);
    void informDone(TaskPtr ptr);
    bool resumeIfNothingToWait(TaskPtr &ptr);
    
    ~TaskGroup();

    int debugId = ++debugId_counter;
    std::atomic<int>    blocking_count = {0};
private:
    friend class Task;
    TaskPtr blockedTask;
    Spinlock            mut_;

    static std::atomic<int> debugId_counter;
};

#endif /* _TASKGROUP_HH_ */

