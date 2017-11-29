#include "co_user.hh"
#include "stdio.h"

#include <chrono>
#include <string>
#include <atomic>
#include <vector>
#include <algorithm>
#include <numeric>
#include <exception>


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
            printf("<%s> duration: %-9.3lfms, %lf ns/op\n", message.c_str(), 
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
                    go([&sum] () {
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

    int sum = 0;
    {
        char msg[128];
        snprintf(msg, 128, "YamiThread:massive_creation:coroutine: %-8ld", size);
        TimeInterval __(msg, size);
        for ( long i = 0; i < size; i++ ) {
            go([&sum, &latch] () {
                    sum ++;
                    latch.down();
                });
        }
        latch.wait();
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
        snprintf(msg, 128, "YamiThread:massive_yield:coroutine: %-6d", coro);
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

#include "Config.hh"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

const int M = 1600;
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
        snprintf(msg, 128, "YamiThread:DenseMatMut:coroutine:%-4d", ntasks);

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
            .registe(go(std::bind(massive_yield_test, 1)))
            .wait()
            ;
    
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
            .registe(go(std::bind(massive_creation_test, 1000)))
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
            .registe(go(std::bind(dense_mat_mut_test, Config::Instance().num_of_threads)))
            .wait()
            ;

        TaskBundle()
            .registe(go(std::bind(dense_mat_mut_test, 40)))
            .wait()
            ;

        TaskBundle()
            .registe(go(std::bind(dense_mat_mut_test, 400)))
            .wait()
            ;

        TaskBundle()
            .registe(go(std::bind(dense_mat_mut_test, 800)))
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
                math_problem1(0L, 1000L, 1000L);
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
    run_benchmarks();
//    setup_at_once();
//    setup(dense_mat_mut_test, Config::Instance().num_of_threads);
//    setup(std::bind(math_problem1, 0L, 1000L, 1000L));
//    setup(std::bind(massive_yield_test, 10000));
//    setup(std::bind(massive_creation_test, 1000000));
//    {
//        TimeInterval("hell");
//    }
//    printf("done...\n");
}
