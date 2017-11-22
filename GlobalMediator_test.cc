#include "GlobalMediator.hh"
#include "Task.hh"
#include "TaskGroup.hh"
#include "util.hh"

#include <memory>
#include <stdio.h>
#include <thread>
#include <chrono>

constexpr int matsz = 10000;
constexpr int num_of_tasks = 250;
constexpr int chunk = matsz / num_of_tasks;

void co_main2() {
    std::vector<int> mat(matsz * matsz);
    std::vector<int> x(matsz);
    std::vector<int> res(matsz);

    for ( int i = 0; i < matsz; i++ ) {
        mat[i * matsz + i] = 2;
        x[i] = i;
    }

    int *mat_ = &mat[0], *x_ = &x[0], *res_ = &res[0];

    TaskGroup group;
    for ( int i = 0; i < num_of_tasks; ++i ) {
        TaskPtr ptr = std::make_shared<Task>(
            [i, mat_, x_, res_] () -> void {
                for ( int j = 0; j < chunk; ++j ) {
                    res_[chunk * i + j] = 0;
                    for ( int k = 0; k < matsz; ++k ) {
                        res_[chunk * i + j] += mat_[(chunk * i + j) * matsz + k] * x_[k];
                    }
                }
            });
        globalMediator.addRunnable(ptr);
        group.registe(ptr);
    }

    group.wait();

    for ( int i = 0; i < matsz; i++ ) {
        printf("%d ", res[i]);
    }
    printf("\n");
}

void co_main1(int s) {
    printf("co_main1 in %d...\n", s);

    TaskGroup group;
    for ( int i = 0; i < 5; i++ ) {
        TaskPtr ptr = std::make_shared<Task>(
                    [i] () -> void {
                        printf("running i = %d\n", i);
                    }
                );
        globalMediator.addRunnable(ptr);
        group.registe(ptr);
    }

    group.wait();
}

void co_main_() {
    //for ( int i = 0; i < 1000000; i++ ) co_main1(i);
    co_main2();
    globalMediator.TerminateGracefully();
}

void test() {
    GlobalMediator::Init();

    printf("Init() done...\n");

    globalMediator.addRunnable(std::make_shared<Task>(co_main_));
    globalMediator.run();
}


int main() {
    test();

//    std::this_thread::sleep_for(std::chrono::milliseconds(2000));

    printf("done...\n");
}
