#include "co_user.hh"
#include "stdio.h"

#include <chrono>
#include <string>
#include <atomic>
#include <vector>
#include <algorithm>
#include <numeric>

constexpr int N = 5000000;

struct TimeInterval {
    std::chrono::time_point<std::chrono::high_resolution_clock>  start;
    std::string message;
    int nops;
    explicit TimeInterval(std::string const &message = "unknown", int nops = N )
        : message(message)
        , nops(nops)
    {
        start = std::chrono::high_resolution_clock::now();
    }
    ~TimeInterval() {
        auto period = std::chrono::high_resolution_clock::now() - start;
        printf("<%s> duration: %lfms, %lf ns/op\n", message.c_str(), 
                std::chrono::duration<double, std::milli>(period).count(),
                (double)std::chrono::duration_cast<std::chrono::nanoseconds>(period).count() / nops
              );
    }
};

void test() {
    TaskBundle bundle;

    for ( int i = 0; i < 10; i++ ) {
        bundle.registe(
                go([i] (int a) {
                    printf("hello at %d: %d.\n", i, a);
                    co_yield;
                    printf("hello at %d: %d, after yield.\n", i, a);
                }, (100 + i)))
              .registe(
                go_pure([i] () {
                    printf("hello not yield %d.\n", i);
                }))
              ;
    }

    bundle.wait();
}

/* empty function benchmark */
void bench1_naive(int size) {
    TaskBundle bundle;

    int sum = 0;
    {
        TimeInterval __("bench1_naive:size:" + std::to_string(size));
        for ( int i = 0; i < size; i++ ) {
            bundle.registe(
                    go([&sum] () {
                        sum ++;
                        })
                    );
        }
        bundle.wait();
        printf("%d\n", sum);
    }
}

void bench1(long size) {
    CountDownLatch latch;
    latch.add(size);

    int sum = 0;
    {
        TimeInterval __("bench1:size:" + std::to_string(size), size);
        for ( long i = 0; i < size; i++ ) {
            go([&sum, &latch] () {
                    sum ++;
                    latch.down();
                });
        }
        latch.wait();
        printf("%d\n", sum);
    }
}


void
bench2(int coro)
{
    CountDownLatch latch;
    latch.add(coro);
    {
        TimeInterval __("bench2:size:" + std::to_string(coro) + ":N:" + std::to_string(N));

        for ( int i = 0; i < coro; ++i ) {
            go( [i, coro, &latch] () {
                for ( int j = 0; j < N / coro; ++j ) {
//                    printf("id %d turn %d\n", i, j);
                    co_yield;
                }
                latch.down();
            });
        }

        latch.wait();
    }
}

void
math_problem1(long from, long gap, long num)
{
    FOR_N_TIMES(100000) {
        go ([from, gap, num] () {
            CountDownLatch latch;
            latch.add(num);
        
            std::vector<long> ress(num);
        
            for ( long i = 0; i < num; ++i ) {
                go( [&ress, &latch, from, gap, i] () {
                    for ( long k = from + gap * i; k < from + gap * (i + 1); ++k ) {
                        ress[i] += k;
                    }
                    latch.down();
                });
            }
        
            latch.wait();
        
            /* pitfal, if 0L not specified, template deduction to 0 int, integer overflow */
            long total = std::accumulate(ress.begin(), ress.end(), 0L);
//            printf("from %ld, gap %ld, num %ld: total: %ld\n",
//                    from, gap, num, total);
        });
    }
}

void
setup(std::function<void()> co_main)
{
    co_init();

    go([co_main] () {
        co_main();
        co_terminate();
        });

    co_mainloop();
}

int
main()
{
//    setup(std::bind(math_problem1, 0L, 1000L, 1000L));
//    setup(std::bind(bench2, 10000));
    setup(std::bind(bench1, 1000000));
//    {
//        TimeInterval("hell");
//    }
    printf("done...\n");
}
