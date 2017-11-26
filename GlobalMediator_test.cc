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

/* compute res = mat * x with automatic task dispatching */
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
        bool isPure = i % 2 ? true : false;
        TaskPtr ptr = makeRefPtr<Task>(
            [i, mat_, x_, res_, isPure] () -> void {
                for ( int j = 0; j < chunk; ++j ) {
                    res_[chunk * i + j] = 0;
                    if ( !isPure && j == chunk / 2 ) {
                        co_yield;
                    }
                    for ( int k = 0; k < matsz; ++k ) {
                        res_[chunk * i + j] += mat_[(chunk * i + j) * matsz + k] * x_[k];
                    }
                }
            }, isPure);
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
//    printf("co_main1 in %d...\n", s);

    std::vector<TaskPtr> someTasks;

    TaskGroup group;
    for ( int i = 0; i < 5; i++ ) {
        TaskPtr ptr = makeRefPtr<Task>(
                    [i] () -> void {
    //                    printf("running i = %d\n", i);
                    }
                );
        if ( i < 3 ) {
            someTasks.push_back(ptr);
        }

        globalMediator.addRunnable(ptr);
        group.registe(ptr);
    }

    /* A pitfall in shared-stack: if one coroutine creates a local var v, and spawn a new coroutine
     * with v captured. It is ok syntactically, but when the child coroutine is running, the 
     * parent's stack was cleanup to place the child, thus child holds a wrong pointer
     */
    FOR_N_TIMES(20) {
        globalMediator.addRunnable(
                makeRefPtr<Task>(
                    // some tasks are waited by multiple groups
                    // must catch by value, because the outer scope might
                    // terminate sooner than the inner task
                    [someTasks] () mutable -> void {
                        TaskGroup group_;
                        for ( auto &ptr : someTasks ) {
                            group_.registe(ptr);
                        }

                        for ( int k = 0; k < 4; ++k ) {
                            TaskPtr ptr = makeRefPtr<Task>(
                                    [k] () -> void {
        //                                printf("inner running: k = %d\n", k);
                                    }, true);
                            group_.registe(ptr);
                            globalMediator.addRunnable(ptr);
                        }
        //                printf("Bonjour... Start waiting\n");
                        group_.wait();
        //                printf("Au revoir\n");
                }));
    }
    group.wait();
}

void co_main_() {
//    for ( int i = 0; i < 500000; i++ ) co_main1(i);
    co_main2();
    GlobalMediator::TerminateGracefully();
}

void test() {
    GlobalMediator::Init();

    printf("Init() done...\n");

    globalMediator.addRunnable(makeRefPtr<Task>(co_main_));
    globalMediator.run();
}


int main() {
    test();

//    std::this_thread::sleep_for(std::chrono::milliseconds(2000));

    printf("done...\n");
}
