#ifndef _SPINLOCK_HH_
#define _SPINLOCK_HH_

#include <atomic>

class Spinlock {
public:
    void lock() {
        while ( locked.test_and_set() )
            ; /* spin */
    }
    void unlock() {
        locked.clear();
    }
private:
    volatile std::atomic_flag locked = ATOMIC_FLAG_INIT;
};

#endif /* _SPINLOCK_HH_ */

