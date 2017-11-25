#ifndef _OBJECTPOOL_HH_
#define _OBJECTPOOL_HH_

#include "debug.hh"
#include "util.hh"
#include "Spinlock.hh"

#include <new>
#include <memory>
#include <array>
#include <mutex>


//#include <stdio.h>
//#define DEBUG_PRINT_LOCAL(fmt, ...) \
//    fprintf(stderr, fmt "\n", ##__VA_ARGS__)

#define DEBUG_PRINT_LOCAL(fmt, ...)


template<class T, class LockType = Spinlock>
class ObjectLayer : public NonCopyable {
public:
    enum {
        NotHere,
        HereButNotMultiple,
        
        /* successfully reclaimed */
        NotInUseAfter,
        StillInUseAfter,
    };

    struct FakeT_ {
        char fake_[sizeof(T)];
    };
    struct FakeEntry {
        union {
            FakeT_ fakeT_;
            FakeEntry *next;
        };
    };

    explicit ObjectLayer(std::size_t sz)
        : arena_cap(sz)
        , arena(new FakeEntry[sz])
        , used(0)
    {
        MUST_TRUE(sz > 0, "ObjectLayer size must >0 ");

        head = arena;
        for ( std::size_t i = 0; i < sz - 1; i++ ) {
            arena[i].next = &arena[i+1];
        }
        arena[sz - 1].next = nullptr;
    }

    ~ObjectLayer() {
        delete[] arena;
    }

    void *alloc() {
        std::lock_guard<LockType> _(mut_);
        if ( !head ) return nullptr;

        void *res = head;

        DEBUG_PRINT_LOCAL("alloc: %p, used = %lu", res, used);

        head = head->next;
        ++used;
        return res;
    }

    /* return isEmpty after reclaim */
    int reclaim(void *ptr) {
        std::lock_guard<LockType> _(mut_);
        if ( !ptr ) {
            DEBUG_PRINT_LOCAL("reclaim nullptr...");
            return used == 0 ? NotInUseAfter : StillInUseAfter;
        }

        if ( (char*)ptr < (char*)arena || (char*)ptr >= (char*)(arena + arena_cap) ) {
            return NotHere;
        }

        if ( ((char*)ptr - (char*)arena) % sizeof(FakeEntry) != 0 ) {
            return HereButNotMultiple;
        }

        DEBUG_PRINT_LOCAL("reclaim: %p, used = %lu", ptr, used);
        ((FakeEntry*)ptr)->next = head;
        head = (FakeEntry*)ptr;
        return --used == 0 ? NotInUseAfter : StillInUseAfter;
    }

private:
    FakeEntry       *arena;
    FakeEntry       *head;

    /* in number-of-object, NOT byte */
    std::size_t     arena_cap;
    std::size_t     used;
    LockType        mut_;
};

template<class T, class LockType = Spinlock>
class ObjectPool : public NonCopyable {
public:
    ObjectPool(std::size_t pool_init_size, std::size_t enlarge_rate)
        : pool_init_size(pool_init_size)
        , enlarge_rate(enlarge_rate)
    {
        layers[0] = std::make_unique<ObjectLayer<T>>(pool_init_size);
    }

    void *alloc() {
        std::lock_guard<LockType> _(mut_);
        void *res;
        for ( int i = current_layer; i >= 0; --i ) {
            if ( (res = layers[i]->alloc()) != nullptr ) {
                DEBUG_PRINT_LOCAL("allocated %p from layer %i", res, i);
                return res;
            }
        }

        // all layers are full 
        current_size *= enlarge_rate;
        layers[++current_layer] = std::make_unique<ObjectLayer<T>>(current_size);
        res = layers[current_layer]->alloc();

        DEBUG_PRINT_LOCAL("all layers are full, increase current_layer to %d, allocated %p",
                current_layer, res);
        return res;
    }

    void reclaim(void *ptr) {
        std::lock_guard<LockType> _(mut_);
        for ( int i = current_layer; i >= 0; --i ) {
            int res = layers[i]->reclaim(ptr);
            if ( res == ObjectLayer<T>::NotHere ) continue;

            if ( res == ObjectLayer<T>::HereButNotMultiple ) {
                CANNOT_REACH_HERE("reclaim a ptr %p which was allocated by some ObjectLayer but with wrong starting address", ptr);
            }

            /* reclaimed successfully at this layers */
            DEBUG_PRINT_LOCAL("ptr %p reclaimed to layer %d", ptr, i);
            if ( res == ObjectLayer<T>::NotInUseAfter && i > 0 && i == current_layer ) {
                /* this layer can be freed */
                DEBUG_PRINT_LOCAL("top layer %i is empty after reclaim of %p, free it", i, ptr);
                layers[i] = nullptr;
                current_layer = i - 1;
                current_size /= enlarge_rate;
            }
            return;
        }

        CANNOT_REACH_HERE("reclaim a ptr %p which was not allocated by ObjectPool", ptr);
    }

    ~ObjectPool() {
        for ( int i = 0; i <= current_layer; ++i ) {
            layers[i] = nullptr;
        }
    }
private:
    std::size_t pool_init_size;
    std::size_t enlarge_rate;

    /* (2^32 * init_size) objects is already insane... */
    std::array<std::unique_ptr<ObjectLayer<T>>, 32> layers;
    int current_layer = 0;
    int current_size = pool_init_size;

    LockType mut_;
};

// template<class T>
// struct Allocator {
//     using size_type = std::size_t;
//     using difference_type = std::ptrdiff_t;
// 
//     using pointer = T*;
//     using const_pointer = const T*;
// 
//     using reference = T&;
//     using const_reference = const T&;
// 
//     using value_type = T;
// 
//     template<class U>
//     struct rebind {
//         using other = Allocator<U>;
//     };
// 
//     Allocator() = default;
//     Allocator(const Allocator&) = default;
// };

#endif /* _OBJECTPOOL_HH_ */

