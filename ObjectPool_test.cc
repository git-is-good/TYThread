#include "ObjectPool.hh"
#include "util.hh"

#include <stdio.h>
#include <array>
#include <memory>
#include <string>
#include <cassert>
#include <atomic>
#include <thread>
#include <chrono>

struct Kitty {
    std::string name;
    int age;
    double weight;

    Kitty(std::string const &name = "<kitty>", int age = 4,
            double weight = 1.445)
        : name(name)
        , age(age)
    {}
    static void* operator new(std::size_t sz);
    static void operator delete(void *ptr);
};

/*
ObjectPool<Kitty> kitty_pool(16, 2);

void*
Kitty::operator new(std::size_t sz)
{
    void *res = kitty_pool.alloc();
    assert(res);
    printf("allocated: %p\n", res);
    return res;
}

void
Kitty::operator delete(void *ptr)
{
    kitty_pool.reclaim(ptr);
    printf("deleted %p\n", ptr);
}

void test() {
    std::unique_ptr<Kitty> ptrs[150];
//    std::shared_ptr<Kitty> ptrs[150];
    for ( int i = 0; i < 150; i++ ) {
        ptrs[i] = std::make_unique<Kitty>("WowKitty", 12);
//        ptrs[i] = std::make_shared<Kitty>("Wolf", 943);
    }
}
*/

std::atomic<int> thread_counter = {0};
std::unique_ptr<ObjectPoolMediator<Kitty>> mediator;
thread_local int thread_id = thread_counter++;

constexpr int sz = 8;
constexpr int num_of_thread = 4;

void init_mediator() {
    ObjectPoolConfig config;
    config
        .set_num_of_thread(num_of_thread)
        .set_pool_init_size(4)
        .set_enlarge_rate(2)
        .set_max_total_cached(4)
        ;
    mediator = std::make_unique<ObjectPoolMediator<Kitty>>(config);
}

void*
Kitty::operator new(std::size_t sz)
{
    void *res = mediator->my_alloc(thread_id);
    assert(res);
    printf("Thread %d allocated: %p\n", thread_id, res);
    return res;
}

void
Kitty::operator delete(void *ptr)
{
    mediator->my_release(thread_id, ptr);
    printf("Thread %d deleted %p\n", thread_id, ptr);
}

void test2() {
    init_mediator();

    std::array<std::thread, num_of_thread - 1> threads;

    std::array<std::unique_ptr<Kitty>, sz * num_of_thread> kittys;

    for ( int i = 0; i < sz; ++i ) {
        kittys[thread_id * sz + i] = std::make_unique<Kitty>("HelloKitty:" + std::to_string(thread_id) + "<>", i);
    }

    for ( int k = 0; k < num_of_thread - 1; ++k ) {
        threads[k] = std::thread(
                [&kittys] () {
                    for ( int i = 0; i < sz; i++ ) {
                        kittys[thread_id * sz + i] = std::make_unique<Kitty>("HelloKitty:" + std::to_string(thread_id) + "<>", i);
                    }
                    std::this_thread::sleep_for( std::chrono::milliseconds(1000) );
                    int to_del = (thread_id + 1) % num_of_thread;
                    for ( int i = 0; i < sz; i++ ) {
                        kittys[to_del * sz + i] = nullptr;
                    }
                }); 

        std::this_thread::sleep_for( std::chrono::milliseconds(1000) );
        int to_del = (thread_id + 1) % num_of_thread;
        for ( int i = 0; i < sz; i++ ) {
            kittys[to_del * sz + i] = nullptr;
        }
    }

    for ( int k = 0; k < num_of_thread - 1; ++k ) {
        threads[k].join();
    }
}

void test() {
    init_mediator();

    std::array<std::thread, num_of_thread - 1> threads;

    std::unique_ptr<Kitty> ptrs[sz];
    for ( int i = 0; i < sz; i++ ) {
        ptrs[i] = std::make_unique<Kitty>("WowKitty", 12);
    }

    for ( int k = 0; k < num_of_thread - 1; ++k ) {
        threads[k] = std::thread([] () {
            std::unique_ptr<Kitty> ptrs[sz];
            for ( int i = 0; i < sz; i++ ) {
                ptrs[i] = std::make_unique<Kitty>("WowKitty", 12);
            }
        });
    }

    for ( int k = 0; k < num_of_thread - 1; ++k ) {
        threads[k].join();
    }
}

int main() {
    //test();
    test2();
}
