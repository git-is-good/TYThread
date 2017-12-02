#include "co_user.hh"
#include "stdio.h"
#include <chrono>
#include <string>
#include <vector>
#include <numeric>
#include <functional>

struct TimeInterval {
    std::chrono::time_point<std::chrono::high_resolution_clock>  start;
    std::string message;
    explicit TimeInterval(std::string const &message = "unknown")
        : message(message)
    {
        start = std::chrono::high_resolution_clock::now();
    }
    ~TimeInterval() {
        auto period = std::chrono::high_resolution_clock::now() - start;
        printf("<%s> duration: %lf ms\n", message.c_str(), 
                    std::chrono::duration<double, std::milli>(period).count());
    }
};

void skynet(long &res, int num, int size, int div) {
    if ( size == 1 ) {
        res = num;
        return;
    }

    std::vector<long> rc(div);
    CountDownLatch latch;
    latch.add(div);
    for ( int i = 0; i < div; i++ ) {
        int subNum = num + i * (size / div);
        go([&latch, &rc, i, subNum, size, div] () {
                skynet(rc[i], subNum, size/div, div);
                latch.down();
            });
    }

    latch.wait();

    res = std::accumulate(rc.begin(), rc.end(), 0L);
}

int main() {
    co_init();

    go([] () {
            {
                TimeInterval __("Skynet");
                long res = 0;
                TaskBundle()
                    .registe(go([&res] () {
                                skynet(res, 0, 1000000, 10);
                            }))
                    .wait()
                    ;

                printf("Result: %ld\n", res);
            }
            co_terminate();
    });

    co_mainloop();
}
