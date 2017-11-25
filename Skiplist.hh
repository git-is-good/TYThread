#ifndef _SKIPLIST_HH_
#define _SKIPLIST_HH_

#include <list>
#include <vector>
#include <utility>
#include "Spinlock.hh"

template<
    class T,
    class LockType = Spinlock,
    int SkipGap = 128,
    int Level = 3>
class Skiplist {
public:

    Skiplist dequeue_half() {
        if ( skiper__.empty() ) {
            if ( incompleteCellular__.empty() ) {
                return std::move(Skiplist{});
            } else {
                int remain_num = skiper__.size() / 2;
                Skiplist res;
                res.skiper__.resize(skiper__.size() - remain_num);
                for ( int i = remain_num; i < skiper__.size(); i++ ) {
                    res[i - remain_num] = std::move(skiper__[i]);
                }
                skiper__.resize(remain_num);
                return std::move(res);
            }
        } else {



        }
    }

private:
    using Cellular = std::list<T>;
    using Skiper = std::vector<Cellular>;

    Skiper skiper__;
    Cellular incompleteCellular__;
};

#endif /* _SKIPLIST_HH_ */

