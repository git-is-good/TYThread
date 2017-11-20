#ifndef _UTIL_HH_
#define _UTIL_HH_

struct NonCopyable {
    using self_type = NonCopyable;

    NonCopyable() = default;

    /* cannot copy */
    NonCopyable(const self_type&) = delete;
    self_type &operator=(const self_type&) = delete;

    /* move is ok */
    NonCopyable(self_type &&) = default;
    self_type &operator=(self_type &&) = default;
};

struct Singleton : public NonCopyable {
    using self_type = Singleton;

    Singleton() = default;

    /* cannot move */
    Singleton(self_type &&) = delete;
    self_type &operator=(self_type &&) = delete;
};

#endif /* _UTIL_HH_ */

