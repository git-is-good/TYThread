#ifndef _UTIL_HH_
#define _UTIL_HH_

#include <atomic>
#include <type_traits>
#include <cstddef>
#include <utility>

#define FOR_N_TIMES(n) for ( int _M_G_C_X_C8377 = 0; _M_G_C_X_C8377 < (n); ++_M_G_C_X_C8377 )

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

/* a class inherit RefCounted, then it has a internal reference counter,
 * such class can only be used as a pointer
 */
class RefCounted : public NonCopyable {
public:
    ~RefCounted() = default;
protected:
    RefCounted()
        : counter__(0)
    {}
private:
    template<class Derived> friend class DerivedRefPtr;
    std::atomic<unsigned int> counter__;
};

template<class Derived>
class DerivedRefPtr {
    static_assert(std::is_base_of<RefCounted, Derived>::value,
            "DerivedRefPtr only accepts derived class of RefCounted");
public:
    DerivedRefPtr()
        : real_ptr__(nullptr)
    {}

    DerivedRefPtr(std::nullptr_t)
        : real_ptr__(nullptr)
    {}

    /* shared_from_this */
    explicit DerivedRefPtr(Derived *ptr) {
        increase(ptr);
        real_ptr__ = ptr;
    }

    ~DerivedRefPtr() {
        decrease(real_ptr__);
    }

    DerivedRefPtr(DerivedRefPtr const &ptr)
        : real_ptr__(ptr.real_ptr__)
    {
        increase(ptr.real_ptr__);
    }

    DerivedRefPtr &operator =(DerivedRefPtr const &ptr) {
        increase(ptr.real_ptr__);
        decrease(real_ptr__);
        real_ptr__ = ptr.real_ptr__;
        return *this;
    }
    DerivedRefPtr &operator =(std::nullptr_t) {
        decrease(real_ptr__);
        real_ptr__ = nullptr;
        return *this;
    }

    DerivedRefPtr(DerivedRefPtr &&ptr)
        : real_ptr__(ptr.real_ptr__)
    {
        ptr.real_ptr__ = nullptr;
    }

    DerivedRefPtr &operator =(DerivedRefPtr &&ptr) {
        decrease(real_ptr__);
        real_ptr__ = ptr.real_ptr__;
        ptr.real_ptr__ = nullptr;
        return *this;
    }

    bool operator==(DerivedRefPtr const &other) const {
        return real_ptr__ == other.real_ptr__;
    }

    bool operator==(std::nullptr_t) const {
        return real_ptr__ == nullptr;
    }

    template<class U>
    bool operator!=(U &&other) const {
        return !this->operator==(std::forward<U>(other));
    }

    operator bool() const {
        return real_ptr__ != nullptr;
    }

    Derived &operator*() {
        return *real_ptr__;
    }
    Derived const &operator*() const {
        return *real_ptr__;
    }

    Derived *operator->() {
        return real_ptr__;
    }
    Derived const *operator->() const {
        return real_ptr__;
    }

    /* only for implementing data structure,
     * where shared pointer semantics is maintained
     * by programmer */
    Derived *get() {
        return real_ptr__;
    }
    Derived const  *get() const {
        return real_ptr__;
    }
    void set(Derived *ptr) {
        real_ptr__ = ptr;
    }
    static void decrease(Derived *ptr) {
        if ( ptr && --ptr->counter__ == 0 ) {
            delete ptr;
        }
    }
    static void increase(Derived *ptr) {
        if ( ptr ) {
            ++ptr->counter__;
        }
    }

private:
    Derived     *real_ptr__;
};

template<class Derived, class... Args>
DerivedRefPtr<Derived>
makeRefPtr(Args&&... args)
{
    return std::move(DerivedRefPtr<Derived>(new Derived(std::forward<Args>(args)...)));
}

#endif /* _UTIL_HH_ */

