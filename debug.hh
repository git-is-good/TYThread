#ifndef _DEBUG_HH_
#define _DEBUG_HH_

#include <stdio.h>

enum {
    DEBUG_Task          = (1 << 0),
    DEBUG_TaskGroup     = (1 << 1),
    DEBUG_PerThreadMgr  = (1 << 2),
};

#define current_debeg_setting (DEBUG_Task | DEBUG_TaskGroup | DEBUG_PerThreadMgr)

#define DEBUG_PRINT(option, fmt, ...)               \
    do {                                            \
        if ( (option) & (current_debeg_setting) ) { \
            fprintf(stderr, "%s:%d:%s: " fmt "\n",  \
                    __FILE__, __LINE__, __func__,   \
                    ##__VA_ARGS__);                 \
        }                                           \
    } while (0)

#endif /* _DEBUG_HH_ */

