#include "co_user.hh"
#include "stdio.h"

#include <chrono>
#include <string>
#include <atomic>

struct TimeInterval {
    std::chrono::time_point<std::chrono::high_resolution_clock>  start;
    std::string message;
    explicit TimeInterval(std::string const &message = "unknown")
        : message(message)
        , start(std::chrono::high_resolution_clock::now())
    {}
    ~TimeInterval() {
        printf("<%s> duration: %lfms\n", message.c_str(), 
                std::chrono::duration<double, std::milli>(
                    std::chrono::high_resolution_clock::now() - start
                    ).count()
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

void bench1(int size) {
    CountDownLatch latch;
    latch.add(size);

    int sum = 0;
    {
        TimeInterval __("bench1:size:" + std::to_string(size));
        for ( int i = 0; i < size; i++ ) {
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
    setup(std::bind(bench1, 1000000));
//    {
//        TimeInterval("hell");
//    }
    printf("%lu\n", sizeof(Task));
    printf("done...\n");
}
