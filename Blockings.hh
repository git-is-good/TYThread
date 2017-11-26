#ifndef _BLOCKINGS_HH_
#define _BLOCKINGS_HH_

#include <mutex>
#include <condition_variable>
#include <utility>
#include <type_traits>
#include <deque>
#include <set>
#include <unordered_set>
#include <vector>

#include "Spinlock.hh"
#include "debug.hh"

template<class T>
struct set_wrapper {
    using type = std::set<T>;
};

template<class T>
struct unordered_set_wrapper {
    using type = std::unordered_set<T>;
};

template<
    class T,
    template<typename> class SetContainer = unordered_set_wrapper,
    class LockType = std::mutex>
class MultiplexingSet {
public:
    using container_type = typename SetContainer<T>::type;
    using value_type = T;

    void ready(value_type const &e) {
        std::lock_guard<LockType>   lock(mut_);
        container.insert(e);
    }

    void ready(value_type &&e) {
        std::lock_guard<LockType>   lock(mut_);
        container.insert(std::move(e));
    }

    value_type multiplex() {
        std::unique_lock<LockType>   lock(mut_);
        while ( container.empty() ) {
            cond_.wait(lock);
        }
        value_type res = *container.begin();
        container.erase(container.begin());
        return res;
    }
private:
    container_type              container;
    LockType                    mut_;
    std::condition_variable     cond_;
};

template<class T>
struct deque_wrapper {
    using type = std::deque<T>;
};

template<
    class T,
    class LockType = Spinlock>
class BlockingQueue {
public:
    using container_type = std::vector<T>;
    using value_type = T;

    void enqueue(value_type const &e) {
        std::lock_guard<LockType>   _(mut_);
        container.push_back(e);
    }

    template<class Iterator>
    void enqueue_by_move(Iterator first, Iterator last) {
        std::lock_guard<LockType>   _(mut_);
        while ( first != last ) {
            container.push_back(std::move(*first));
            ++first;
        }
    }

    std::pair<bool, T> try_dequeue() {
        std::lock_guard<LockType>     _(mut_);
        if ( container.empty() ) {
            return std::make_pair(false, T{});
        } else {
            T res = std::move(container.back());
            container.pop_back();
            return std::make_pair(true, std::move(res));
        }
    }

    std::pair<bool, std::vector<T>> try_dequeue_half() {
        std::lock_guard<LockType>       _(mut_);
        DEBUG_PRINT(DEBUG_Blockings,
                "original size: %lu", container.size());
        if ( container.empty() ) {
            return std::make_pair(false, std::vector<T>{});
        } else {
            std::size_t sz = (container.size() + 1) / 2;
            std::size_t remain_sz = container.size() - sz;
            std::vector<T> res(sz);
            for ( size_t i = remain_sz; i < container.size(); i++ ) {
                res[i - remain_sz] = std::move(container[i]);
            }
            container.resize(remain_sz);
            DEBUG_PRINT(DEBUG_Blockings,
                    "after size: %lu, vec size: %lu, sz: %lu", container.size(), res.size(), sz);
            return std::make_pair(true, std::move(res));
        }
    }
private:
    container_type              container;
    LockType                    mut_;
};

template<
    class T,
    template<typename> class QueueContainer = deque_wrapper,
    class LockType = Spinlock>
class BlockingQueue_ {
public:
    using container_type = typename QueueContainer<T>::type;
    using value_type = T;

    void enqueue(value_type const &e) {
        std::lock_guard<LockType>     lock(mut_);
        container.push_back(e);
    }

    template<class Iterator>
    void enqueue_by_move(Iterator first, Iterator last) {
        std::lock_guard<LockType>   lock(mut_);
        while ( first != last ) {
            container.push_back(std::move(*first));
            ++first;
        }
    }

    void enqueue(value_type &&e) {
        std::lock_guard<LockType>     lock(mut_);
        container.push_back(std::move(e));
    }

//    T dequeue() {
//        std::unique_lock<LockType>     lock(mut_);
//        while ( container.empty() ) {
//            cond_.wait(lock);
//        }
//
//        T res = std::move(container.front());
//        container.pop_front();
//        return std::move(res);
//    }

    /* if queue is empty, just return false */
    std::pair<bool, T> try_dequeue() {
        std::lock_guard<LockType>     lock(mut_);
        if ( container.empty() ) {
            return std::make_pair(false, T{});
        } else {
            T res = std::move(container.front());
            container.pop_front();
            return std::make_pair(true, std::move(res));
        }
    }

    std::pair<bool, std::vector<T>> try_dequeue_half() {
        std::lock_guard<LockType>       lock(mut_);
        DEBUG_PRINT(DEBUG_Blockings,
                "original size: %lu", container.size());
        if ( container.empty() ) {
            return std::make_pair(false, std::vector<T>{});
        } else {
            std::size_t sz = (container.size() + 1) / 2;
            std::vector<T> res(sz);
            for ( size_t i = 0; i < sz; i++ ) {
                res[i] = std::move(container.front());
                container.pop_front();
            }
            DEBUG_PRINT(DEBUG_Blockings,
                    "after size: %lu, vec size: %lu, sz: %lu", container.size(), res.size(), sz);
            return std::make_pair(true, std::move(res));
        }
    }

private:
    container_type              container;
    LockType                    mut_;
//    std::condition_variable     cond_;
};

#endif /* _BLOCKINGS_HH_ */

