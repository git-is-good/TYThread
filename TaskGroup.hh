#ifndef _TASKGROUP_HH_
#define _TASKGROUP_HH_

#include "util.hh"
#include "forward_decl.hh"

#include <memory>
#include <vector>
#include <mutex>
#include <atomic>

#include <unordered_set>

/* TaskGroup might be accessed by multiple threads
 * through informDone();
 */
class TaskGroup : public NonCopyable {
public:
    TaskGroup();
    void wait();
    TaskGroup &registe(TaskPtr &ptr);
    TaskGroup &registe(TaskPtr &&ptr) {
        registe(ptr);
        return *this;
    }
    void informDone(TaskPtr ptr);
    bool resumeIfNothingToWait(TaskPtr &ptr);
    void pushTaskPtr(TaskPtr &&ptr);
    
    ~TaskGroup();

    int debugId = ++debugId_counter;
//private:
    bool    inCoroutine;
//    bool    blocked = false;

    TaskPtr blockedTask;
    std::vector<TaskPtr> taskPtrs;
//    std::unordered_set<TaskPtr> taskPtrs;
//    std::mutex          mut_;
    Spinlock            mut_;

    static std::atomic<int> debugId_counter;


};

#endif /* _TASKGROUP_HH_ */

