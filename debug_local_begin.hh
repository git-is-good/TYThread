/* if debug_local enabled, define it, otherwise define a fake one */
#ifdef ENABLE_DEBUG_LOCAL

#include <stdio.h>
#define DEBUG_PRINT_LOCAL(fmt, ...)             \
        fprintf(stderr, "%d:%s: " fmt "\n",  \
                 __LINE__, __func__,            \
                ##__VA_ARGS__);                 \


#else 

#define DEBUG_PRINT_LOCAL(fmt, ...)

#endif /* DEBUG_SKIPLIST__ */
