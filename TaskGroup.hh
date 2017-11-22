#ifndef _TASKGROUP_HH_
#define _TASKGROUP_HH_

#include "util.hh"
#include "forward_decl.hh"

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
    bool isWaitingListAreadyEmpty_locked() { return taskPtrs.empty(); }
    
    ~TaskGroup();

    int debugId = ++debugId_counter;
//private:
    TaskPtr blockedTask;
    std::vector<TaskPtr> taskPtrs;
    std::mutex          mut_;

    static std::atomic<int> debugId_counter;


};

#endif /* _TASKGROUP_HH_ */

