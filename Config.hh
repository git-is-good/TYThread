#ifndef _CONFIG_HH_
#define _CONFIG_HH_

#include "util.hh"

struct Config : public Singleton {
    int max_stack_size = 1024 * 128;
    int num_of_threads = 2;

    /* in milliseconds */
    int max_wait_task_time = 500;

    /* very rare case, if a task yields with GroupWait,
     * another thread might wakes it up *before*
     * the switch statement in PerThreadMgr::run_runnable;
     * to avoid race condition, the waking thread should
     * check a flag, when it is not set, polling with
     * this period 
     */
    int continuation_ret_conflict_period = 5;

    static Config &Instance() {
        static Config conf;
        return conf;
    }
};

#endif /* _CONFIG_HH_ */

