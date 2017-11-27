#ifndef _CONFIG_HH_
#define _CONFIG_HH_

#include "util.hh"

struct Config : public Singleton {
    int max_stack_size = 1024 * 128;
    int num_of_threads = 3;

    /* in milliseconds */
    int max_wait_task_time = 5;

    /* Task pool size */
    int init_task_pool_size = 1024;
    int enlarge_rate = 2;

    static Config &Instance() {
        static Config conf;
        return conf;
    }
};

#endif /* _CONFIG_HH_ */

