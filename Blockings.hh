#ifndef _BLOCKINGS_HH_
#define _BLOCKINGS_HH_

#include <mutex>
#include <condition_variable>
#include <utility>
#include <type_traits>
#include <deque>
#include <set>
#include <unordered_set>

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
    template<typename> class QueueContainer = deque_wrapper,
    class LockType = std::mutex>
class BlockingQueue {
public:
    using container_type = typename QueueContainer<T>::type;
    using value_type = T;

    /* isEmptyBefore */
    bool enqueue(value_type const &e) {
        std::lock_guard<LockType>     lock(mut_);
        bool isEmptyBefore = container.empty();

        container.push_back(e);
        return isEmptyBefore;
    }

    bool enqueue(value_type &&e) {
        std::lock_guard<LockType>     lock(mut_);
        bool isEmptyBefore = container.empty();

        container.push_back(std::move(e));
        return isEmptyBefore;
    }

    T dequeue() {
        std::unique_lock<LockType>     lock(mut_);
        while ( container.empty() ) {
            cond_.wait(lock);
        }

        T res = std::move(container.front());
        container.pop_front();
        return std::move(res);
    }

    /* if queue is empty, just return false */
    std::pair<bool, T> try_dequeue() {
        std::lock_guard<LockType>     lock(mut_);
        if ( container.empty() ) {
            return std::make_pair(false, T{});
        } else {
            T res = container.front();
            container.pop_front();
            return std::make_pair(true, move(res));
        }
    }

private:
    container_type              container;
    LockType                    mut_;
    std::condition_variable     cond_;
};

#endif /* _BLOCKINGS_HH_ */

