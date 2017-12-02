#ifndef _CONFIG_HH_
#define _CONFIG_HH_

#include "util.hh"
#include "stdlib.h"
#include <string>

/* mainly for malloc without multithread optimization */
//#define ENABLE_OBJECT_POOL

struct Config : public Singleton {
    int max_stack_size = 512 * 7;
    int num_of_threads = 6;

    /* in microseconds */
    int max_wait_task_time = 1000;

    /* Task pool size */
    int init_task_pool_size = 256;
    int enlarge_rate = 2;
    int max_total_cached = 128;
    /* in milliseconds, 0 for no GC daemon */
    int cache_reclaim_period = 2000;

    Config() {
        char const *env_value;
        if ( (env_value = getenv("YAMI_NUM_OF_THREADS")) != nullptr ) {
            int user_num_of_threads;
            try {
                num_of_threads = std::stoi(env_value);
            } catch (std::exception const&) {
                /* ignore */
            }

            if ( user_num_of_threads > 0 ) {
                num_of_threads = user_num_of_threads;
            }
        }
    }

    static Config &Instance() {
        static Config conf;
        return conf;
    }
};

#endif /* _CONFIG_HH_ */
