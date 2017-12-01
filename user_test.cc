#include "co_user.hh"
#include "stdio.h"

#include <chrono>
#include <string>
#include <atomic>
#include <vector>
#include <algorithm>
#include <numeric>
#include <exception>
#include <iterator>
#include <cstdlib>
#include <ctime>

int fake_sum;
FILE *trash;

struct TimeInterval {
    std::chrono::time_point<std::chrono::high_resolution_clock>  start;
    std::string message;
    int nops;
    explicit TimeInterval(std::string const &message = "unknown", int nops = 0 )
        : message(message)
        , nops(nops)
    {
        start = std::chrono::high_resolution_clock::now();
    }
    ~TimeInterval() {
        auto period = std::chrono::high_resolution_clock::now() - start;
        if ( nops != 0 ) {
            printf("<%s> duration: %-9.3lfms, %-7.3lf ns/op\n", message.c_str(), 
                    std::chrono::duration<double, std::milli>(period).count(),
                    (double)std::chrono::duration_cast<std::chrono::nanoseconds>(period).count() / nops
                  );
        } else {
            printf("<%s> duration: %lfms\n", message.c_str(), 
                    std::chrono::duration<double, std::milli>(period).count());
        }
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
void massive_creation_test_naive(int size) {
    TaskBundle bundle;

    int sum = 0;
    {
        TimeInterval __("massive_creation_test_naive:size:" + std::to_string(size));
        for ( int i = 0; i < size; i++ ) {
            bundle.registe(
                    go_pure([&sum] () {
                        sum ++;
                        })
                    );
        }
        bundle.wait();
    }
}

void massive_creation_test(long size) {
    CountDownLatch latch;
    latch.add(size);

    {
        char msg[128];
        snprintf(msg, 128, "Yami:massive_creation:coroutine: %-8ld", size);
        TimeInterval __(msg, size);
        for ( long i = 0; i < size; i++ ) {
            go([&latch] () {
                    fake_sum ++;
                    latch.down();
                });
        }
        latch.wait();

        /* avoid optimization */
        fprintf(trash, "%d", fake_sum);
    }
}


constexpr int N = 5000000;
void
massive_yield_test(int coro)
{
    CountDownLatch latch;
    latch.add(coro);
    {
        char msg[128];
        snprintf(msg, 128, "Yami:massive_yield:coroutine: %-6d:total_yield: %d", coro, N);
        TimeInterval __(msg, N);

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
complex_scheduling_test(int test_size, int num)
{
    CountDownLatch bigLatch;
    bigLatch.add(test_size);

    {
        char msg[128];
        snprintf(msg, 128, "Yami:complex_scheduling:%-6dx%-6d", test_size, num);
        TimeInterval _(msg, test_size * num);
        FOR_N_TIMES(test_size) {
            go ([&bigLatch, num] () {
                CountDownLatch latch;
                latch.add(num);

                for ( long i = 0; i < num; ++i ) {
                    go( [&latch] () {
                        fake_sum += 57;
                        latch.down();
                    });
                }
                latch.wait();
                bigLatch.down();
            });
        }
        bigLatch.wait();

        /* avoid optimization */
        fprintf(trash, "%d", fake_sum);
    }
}

#include "Config.hh"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

const int M = 2000;
int A[M][M], B[M][M];
int C1[M][M], C2[M][M];

void
dense_mat_mut_test(int ntasks = Config::Instance().num_of_threads)
{
    srand((int)time(NULL));
    for (int i = 0; i < M; ++i) {
        for (int j = 0; j < M; ++j) {
            A[i][j] = rand() % 100;
            B[i][j] = rand() % 100;
        }
    }

//    {
//        TimeInterval __("ompbench:Sequential");
//        for (int i = 0; i < M; ++i) {
//            for (int j = 0; j < M; ++j) {
//                int Sum = 0;
//                for (int k = 0; k < M; ++k) {
//                    Sum += A[i][k] * B[k][j];
//                }
//                C1[i][j] = Sum;
//            }
//        }
//    }

    {
        char msg[128];
        snprintf(msg, 128, "Yami:DenseMatMut:coroutine:%-4d", ntasks);

        TimeInterval __(msg);
        if ( M % ntasks != 0 ) {
            fprintf(stderr, "For benchmark, ntasks should divide M\n");
            std::terminate();
        }

        int chunk = M / ntasks;


        TaskBundle bundle;
        for (int i = 0; i < ntasks; ++i) {
            bundle.registe(
                go ([i, chunk] () {
                    for ( int s = 0; s < chunk; ++s ) {
                        int r = i * chunk + s;
                        for (int j = 0; j < M; ++j) {
                            int sum = 0;
                            for (int k = 0; k < M; ++k) {
                                sum += A[r][k] * B[k][j];
                            }
                            C2[r][j] = sum;
                        }
                    }
                }));
        }
        bundle.wait();
    }

//    printf("Checking result...\n");
//    for (int i = 0; i < M; i++) {
//        for (int j = 0; j < M; j++) {
//            assert (C1[i][j] == C2[i][j]);
//        }
//    }
//    printf("Passed.\n");
}

template<class Iterator>
void
merge_sort(Iterator first, Iterator last, std::size_t min_diff)
{
    auto diff = std::distance(first, last);
    if ( diff < min_diff ) {
        std::stable_sort(first, last);
        return;
    }

    auto middle = first + diff / 2;

    TaskBundle bundle;

//    bundle.registe(go(merge_sort<Iterator>, first, middle, min_diff));

//    co_yield;

//    bundle
//        .registe(go(merge_sort<Iterator>, middle, last, min_diff))
//        .wait()
//        ;

    TaskBundle()
        .registe(go(merge_sort<Iterator>, first, middle, min_diff))
        .registe(go(merge_sort<Iterator>, middle, last, min_diff))
        .wait()
        ;

    std::inplace_merge(first, middle, last);
}

void
merge_sort_test(std::size_t sz, std::size_t min_diff)
{
    std::vector<int> v(sz);
    std::srand(std::time(NULL));

    std::generate(v.begin(), v.end(), std::rand);

    {
        char msg[128];
        snprintf(msg, 128, "Yami:merge_sort:%-10lu:min_diff:%-9lu", sz, min_diff);
        TimeInterval _(msg, sz);
        merge_sort(v.begin(), v.end(), min_diff);
    }

    for ( auto iter = v.begin(); iter + 1 != v.end(); ++iter ) {
        assert(*iter <= *(iter + 1));
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

void
run_benchmarks()
{
    co_init();

    go ( [] () {
        TaskBundle()
            .registe(go(std::bind(massive_yield_test, 10)))
            .wait()
            ;
    
        TaskBundle()
            .registe(go(std::bind(massive_yield_test, 100)))
            .wait()
            ;
    
        TaskBundle()
            .registe(go(std::bind(massive_yield_test, 1000)))
            .wait()
            ;
    
        TaskBundle()
            .registe(go(std::bind(massive_yield_test, 10000)))
            .wait()
            ;
    
        TaskBundle()
            .registe(go(std::bind(massive_yield_test, 100000)))
            .wait()
            ;

        TaskBundle()
            .registe(go(std::bind(massive_creation_test, 10000)))
            .wait()
            ;
    
        TaskBundle()
            .registe(go(std::bind(massive_creation_test, 100000)))
            .wait()
            ;

        TaskBundle()
            .registe(go(std::bind(massive_creation_test, 1000000)))
            .wait()
            ;
    
        TaskBundle()
            .registe(go(std::bind(massive_creation_test, 10000000)))
            .wait()
            ;

        TaskBundle()
            .registe(go(std::bind(complex_scheduling_test, 100, 100000)))
            .wait()
            ;

        TaskBundle()
            .registe(go(std::bind(complex_scheduling_test, 1000, 10000)))
            .wait()
            ;

        TaskBundle()
            .registe(go(std::bind(complex_scheduling_test, 10000, 1000)))
            .wait()
            ;

        TaskBundle()
            .registe(go(std::bind(complex_scheduling_test, 100000, 100)))
            .wait()
            ;

        TaskBundle()
            .registe(go(std::bind(merge_sort_test, 1000000000LU, 100000000LU)))
            .wait()
            ;

        TaskBundle()
            .registe(go(std::bind(merge_sort_test, 1000000000LU, 10000000LU)))
            .wait()
            ;

        TaskBundle()
            .registe(go(std::bind(merge_sort_test, 1000000000LU, 1000000LU)))
            .wait()
            ;

        TaskBundle()
            .registe(go(std::bind(merge_sort_test, 1000000000LU, 100000LU)))
            .wait()
            ;

        TaskBundle()
            .registe(go(std::bind(merge_sort_test, 1000000000LU, 10000LU)))
            .wait()
            ;

        TaskBundle()
            .registe(go(std::bind(merge_sort_test, 1000000000LU, 1000LU)))
            .wait()
            ;

//        TaskBundle()
//            .registe(go(std::bind(dense_mat_mut_test, Config::Instance().num_of_threads)))
//            .wait()
//            ;

        TaskBundle()
            .registe(go(std::bind(dense_mat_mut_test, 10)))
            .wait()
            ;

        TaskBundle()
            .registe(go(std::bind(dense_mat_mut_test, 100)))
            .wait()
            ;

        TaskBundle()
            .registe(go(std::bind(dense_mat_mut_test, 1000)))
            .wait()
            ;

        co_terminate();
    });

    co_mainloop();
}

void
setup_at_once()
{
    co_init();

    go([] () {
        CountDownLatch latch;
        latch.add(4);

        go([&latch] () {
                complex_scheduling_test(10000, 1000);
                latch.down();
        });

        go([&latch] () {
                massive_yield_test(10000);
                latch.down();
        });

        go([&latch] () {
                massive_creation_test(1000000);
                latch.down();
        });

        go([&latch] () {
                dense_mat_mut_test();
                latch.down();
        });

        latch.wait();
        co_terminate();
    });

    co_mainloop();
}

int
main()
{
    trash = fopen("/dev/null", "w");
    assert(trash);
    run_benchmarks();
//    setup(std::bind(merge_sort_test, 1000000000LU, 1000000LU));
//    setup_at_once();
//    setup(std::bind(complex_scheduling_test, 10000, 1000));
//    setup(dense_mat_mut_test, Config::Instance().num_of_threads);
//    setup(std::bind(math_problem1, 0L, 1000L, 1000L));
//    setup(std::bind(massive_yield_test, 10000));
//    setup(std::bind(massive_creation_test, 1000000));
//    {
//        TimeInterval("hell");
//    }
//    printf("done...\n");
}
