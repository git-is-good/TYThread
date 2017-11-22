#ifndef _DEBUG_HH_
#define _DEBUG_HH_

#include <stdio.h>
#include <assert.h>

enum {
    DEBUG_Task              = (1 << 0),
    DEBUG_TaskGroup         = (1 << 1),
    DEBUG_PerThreadMgr      = (1 << 2),
    DEBUG_GlobalMediator    = (1 << 3),
    DEBUG_Blockings         = (1 << 4),
};

#define current_debeg_setting (DEBUG_Task | DEBUG_TaskGroup | DEBUG_PerThreadMgr | DEBUG_GlobalMediator) 
//#define current_debeg_setting 0
//#define current_debeg_setting (DEBUG_Task | DEBUG_TaskGroup | DEBUG_GlobalMediator | DEBUG_Blockings)
//#define current_debeg_setting DEBUG_GlobalMediator

#define DEBUG_PRINT(option, fmt, ...)               \
    do {                                            \
        if ( (option) & (current_debeg_setting) ) { \
            fprintf(stderr, "%s:%d:%s: " fmt "\n",  \
                    __FILE__, __LINE__, __func__,   \
                    ##__VA_ARGS__);                 \
        }                                           \
    } while (0)

#define MUST_TRUE(s, fmt, ...)                      \
    do {                                            \
        if ( s ) break;                             \
        fprintf(stderr, "fatal: " fmt "\n",         \
                ##__VA_ARGS__);                     \
        assert(s);                                  \
    } while (0)

#endif /* _DEBUG_HH_ */

