#ifndef _OBJECTPOOL_HH_
#define _OBJECTPOOL_HH_

#include "debug.hh"
#include "util.hh"
#include "Spinlock.hh"

#include <new>
#include <memory>
#include <array>
#include <vector>
#include <mutex>
#include <thread>

//#define ENABLE_DEBUG_LOCAL
#include "debug_local_begin.hh"

#define set_id_and_layer(some_fakeT, id, layer) \
    (some_fakeT).combined_info = ((static_cast<unsigned>(id) << 16) | static_cast<unsigned>(layer))

#define read_id(some_fakeT)  \
    ((int)((((some_fakeT).combined_info)) >> 16))

#define read_layer(some_fakeT)  \
    ((int)(((some_fakeT).combined_info) & 0xFFFFU))

template<class T>
struct FakeT_ {
    char fake_[sizeof(T)];

    /* leading 16 bits: id_of_creation_pool, last 16 bits: layer */
    unsigned combined_info;

    /* the pool in which it is created */
//    int id_of_creation_pool;

    /* the layer of creation */
//    int layer;
};

template<class T>
struct FakeEntry {
    union {
        FakeT_<T> fakeT_;
        FakeEntry *next;
    };
};

template<class T>
struct PerLayerCache {
    FakeEntry<T>    *head = nullptr;
    FakeEntry<T>    *tail = nullptr;
    int             num_cached = 0;
    void reset() {
        head = tail = nullptr;
        num_cached = 0;
    }
};

template<class T>
class ObjectLayer : public NonCopyable {
public:
    enum {
        NotHere,
        HereButNotMultiple,
        
        /* successfully my_released */
        NotInUseAfter,
        StillInUseAfter,
    };

    explicit ObjectLayer(std::size_t sz)
        : arena_cap(sz)
        , arena(new FakeEntry<T>[sz])
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

    FakeEntry<T> *my_alloc() {
        if ( !head ) return nullptr;

        FakeEntry<T> *res = head;

        DEBUG_PRINT_LOCAL("my_alloc: %p, used = %lu", res, used);

        head = head->next;
        ++used;
        return res;
    }

    /* return isEmpty after my_release */
    int my_release(FakeEntry<T> *ptr) {
        if ( !ptr ) {
            DEBUG_PRINT_LOCAL("my_release nullptr...");
            return used == 0 ? NotInUseAfter : StillInUseAfter;
        }

        if ( (char*)ptr < (char*)arena || (char*)ptr >= (char*)(arena + arena_cap) ) {
            return NotHere;
        }

        if ( ((char*)ptr - (char*)arena) % sizeof(FakeEntry<T>) != 0 ) {
            return HereButNotMultiple;
        }

        DEBUG_PRINT_LOCAL("my_release: %p, used = %lu", ptr, used);
        ptr->next = head;
        head = ptr;
        return --used == 0 ? NotInUseAfter : StillInUseAfter;
    }

    int my_release(PerLayerCache<T> &cache) {
        cache.tail->next = head;
        head = cache.head;
        used -= cache.num_cached;
        cache.reset();

        return used == 0 ? NotInUseAfter : StillInUseAfter;
    }

    bool isInUse() const {
        return used != 0;
    }
private:
    FakeEntry<T>    *arena;
    FakeEntry<T>    *head;

    /* in number-of-object, NOT byte */
    std::size_t     arena_cap;
    std::size_t     used;
};

struct ObjectPoolConfig {
    int num_of_thread;
    std::size_t pool_init_size;
    std::size_t enlarge_rate;
    int max_total_cached = 1024;
    int cache_reclaim_period = 0;

    ObjectPoolConfig &set_num_of_thread(int p) {
        num_of_thread = p;
        return *this;
    }

    ObjectPoolConfig &set_pool_init_size(std::size_t p) {
        pool_init_size = p;
        return *this;
    }

    ObjectPoolConfig &set_enlarge_rate(std::size_t p) {
        enlarge_rate = p;
        return *this;
    }

    ObjectPoolConfig &set_max_total_cached(int p) {
        max_total_cached = p;
        return *this;
    }

    ObjectPoolConfig &set_cache_reclaim_period(int p) {
        cache_reclaim_period = p;
        return *this;
    }
};

template<class T, class LockType>
class ObjectPoolMediator;

/* (2^32 * init_size) objects is already insane... */
template<class T, class LockType, int NLayers = 32>
class ObjectPool : public NonCopyable {
public:
    ObjectPool(ObjectPoolMediator<T, LockType> &mediator, int id, ObjectPoolConfig const &config)
        : mediator(mediator)
        , id(id)
        , num_of_thread(config.num_of_thread)
        , pool_init_size(config.pool_init_size)
        , enlarge_rate(config.enlarge_rate)
        , max_total_cached(config.max_total_cached)
    {
        layers[0] = std::make_unique<ObjectLayer<T>>(pool_init_size);
        caches.resize(num_of_thread);
    }

    void *my_alloc() {
        std::lock_guard<LockType> _(mut_);
        FakeEntry<T> *res;
        for ( int i = current_layer; i >= 0; --i ) {
            if ( (res = layers[i]->my_alloc()) != nullptr ) {
                DEBUG_PRINT_LOCAL("ObjectPool %d, my_allocated %p from layer %i", id, res, i);
                set_id_and_layer(res->fakeT_, id, i);
//                res->fakeT_.id_of_creation_pool = id;
//                res->fakeT_.layer = i;
                return res;
            }
        }

        // all layers are full 
        MUST_TRUE(current_layer + 1 < layers.size(),
                "ObjectPool %d exceeds its maximal layer: %lu", id, layers.size());
        current_size *= enlarge_rate;
        layers[++current_layer] = std::make_unique<ObjectLayer<T>>(current_size);

        res = layers[current_layer]->my_alloc();
        set_id_and_layer(res->fakeT_, id, current_layer);
//        res->fakeT_.id_of_creation_pool = id;
//        res->fakeT_.layer = current_layer;

        DEBUG_PRINT_LOCAL("ObjectPool %d: all layers are full, increase current_layer to %d, my_allocated %p",
                id, current_layer, res);
        return res;
    }

    void my_release(int layer, PerLayerCache<T> &cache) {
        DEBUG_PRINT_LOCAL("ObjectPool %d: releasing cache of layer:%d,num:%d", id,
                layer, cache.num_cached);

        std::lock_guard<LockType>   _(mut_);
        int res = layers[layer]->my_release(cache);
        remove_layer_if_not_in_use(layer, res);
    }

    void my_release(FakeEntry<T> *ptr);

    void do_period_cleanup() {
        std::lock_guard<LockType>   _(my_release_mut_);
        if ( total_cached != 0 ) {
            cleanup_all_cache();
        }
    }
private:
    void remove_layer_if_not_in_use(int layer, int state) {
        if ( state == ObjectLayer<T>::NotInUseAfter && layer > 0 && layer == current_layer ) {
            /* this layer can be freed */
            DEBUG_PRINT_LOCAL("ObjectPool id:%d: top layer %d becomming empty, free it.", id, layer);
            layers[layer] = nullptr;
            current_layer = layer - 1;
            current_size /= enlarge_rate;

            /* maybe the lower layer is also removable */
            while ( current_layer > 0 && !layers[current_layer]->isInUse() ) {
                DEBUG_PRINT_LOCAL("ObjectPool id:%d: top layer %d becomming empty, free it.", id, current_layer);
                layers[current_layer] = nullptr;
                --current_layer;
                current_size /= enlarge_rate;
            }
        }
    }

    void cleanup_all_cache();

    ObjectPoolMediator<T, LockType>     &mediator;
    int                                 id;
    int                                 num_of_thread;

    std::size_t pool_init_size;
    std::size_t enlarge_rate;

    std::array<std::unique_ptr<ObjectLayer<T>>, NLayers> layers;
    int current_layer = 0;
    int current_size = pool_init_size;

    LockType mut_;

    LockType my_release_mut_;

    using PerIdCache = std::array<PerLayerCache<T>, NLayers>;
    std::vector<PerIdCache> caches;    
    int total_cached = 0;
    int max_total_cached;
};

template<class T, class LockType = Spinlock>
class ObjectPoolMediator : public NonCopyable {
public:
    explicit ObjectPoolMediator(ObjectPoolConfig const &config) {
        pools.resize(config.num_of_thread);
        for ( int i = 0; i < config.num_of_thread; ++i ) {
            pools[i] = std::make_unique<ObjectPool<T, LockType>>(*this, i, config);
        }
        if ( config.cache_reclaim_period > 0 ) {
            // create GC daemon
            daemon.run(this, config.cache_reclaim_period);
        }
        DEBUG_PRINT_LOCAL("init done... num_of_thread: %d", config.num_of_thread);
    }

    // called by user
    void *my_alloc(int id) {
        return pools[id]->my_alloc();
    }

    void my_release(int id, void *ptr) {
        pools[id]->my_release(reinterpret_cast<FakeEntry<T>*>(ptr));
    }

    // called by ObjectPool
    void real_my_release(int id, int layer, PerLayerCache<T> &cache) {
        pools[id]->my_release(layer, cache);
    }

    void daemonTerminate() {
        if ( daemon ) {
            daemon.terminate();
        }
    }

    class ObjectPoolGCDaemon {
    public:
        void terminate() {
            terminatable = true;
            daemon_.join();
        };
        operator bool() {
            return configed;
        }
        void run(ObjectPoolMediator *mediator, int cache_reclaim_period) {
            configed = true;
            daemon_ = std::thread( [this, mediator, cache_reclaim_period] () {
                while ( !terminatable ) {
                    for ( auto &pool : mediator->pools ) {
                        pool->do_period_cleanup();
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(cache_reclaim_period));
                }
            });
        }
        ObjectPoolGCDaemon() = default;
    private:
        bool terminatable = false;
        bool configed = false;
        std::thread daemon_;
    };
private:
    std::vector<std::unique_ptr<ObjectPool<T, LockType>>> pools;
    ObjectPoolGCDaemon daemon;
};

template<class T, class LockType, int NLayers>
void
ObjectPool<T, LockType, NLayers>::my_release(FakeEntry<T> *ptr) {
    {
        std::lock_guard<LockType> _(my_release_mut_);
//        if ( ptr->fakeT_.id_of_creation_pool != id ) {
        if ( read_id(ptr->fakeT_) != id ) {
            /* not created by this pool, thus cache it */
            DEBUG_PRINT_LOCAL("ObjectPool %d: cache a ptr %p from pool:%d,layer:%d, total_cached:%d",
                    id, ptr, read_id(ptr->fakeT_), read_layer(ptr->fakeT_), total_cached + 1);

//            PerIdCache &cache = caches[ptr->fakeT_.id_of_creation_pool];
//            PerLayerCache<T> &layer = cache[ptr->fakeT_.layer];
            PerIdCache &cache = caches[read_id(ptr->fakeT_)];
            PerLayerCache<T> &layer = cache[read_layer(ptr->fakeT_)];
            if ( layer.tail ) {
                ptr->next = layer.head;
                layer.head = ptr;
            } else {
                layer.head = layer.tail = ptr;
            }
            ++layer.num_cached;

            /* whether need a real my_release of all cache */
            if ( ++total_cached > max_total_cached ) {
                DEBUG_PRINT_LOCAL("ObjectPool %d: cache full clean up all cache", id);
                cleanup_all_cache();
            }
            return;
        }
    }

    /* created by this pool, my_release directly */
    {
        std::lock_guard<LockType> _(mut_);
//        int layer = ptr->fakeT_.layer;
        int layer = read_layer(ptr->fakeT_);

        DEBUG_PRINT_LOCAL("ObjectPool %d my_release ptr %p", id, ptr);
        int res = layers[layer]->my_release(ptr);
        remove_layer_if_not_in_use(layer, res);
    }
}

template<class T, class LockType, int NLayers>
void
ObjectPool<T, LockType, NLayers>::cleanup_all_cache() {
    for ( int i = 0; i < num_of_thread; ++i ) {
        if ( i == id ) continue;

        for ( int j = 0; j < NLayers; ++j ) {
            if ( caches[i][j].num_cached > 0 ) {
                mediator.real_my_release(i, j, caches[i][j]);
            }
        }
    }
    total_cached = 0;
}

#include "debug_local_end.hh"
#endif /* _OBJECTPOOL_HH_ */
