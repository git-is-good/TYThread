#include "PerThreadMgr.hh"
#include "TaskGroup.hh"
#include "Task.hh"

#include <iostream>
#include <thread>
#include <chrono>

void callback() {
    std::cout << "about to yield 1\n";
    co_yield;

    TaskPtr t3 = std::make_shared<Task>(
            [] () -> void {
                for ( int i = 0; i < 4; i++ ) {
                    std::cout << "t3 running... at " << i << "\n";
                    co_yield;
                }
            });

    TaskPtr t4 = std::make_shared<Task>(
            [] () -> void {
                std::cout << "t4 run and return\n";
            });

    globalTaskMgr.addRunnable(t3);
    globalTaskMgr.addRunnable(t4);

    TaskGroup gp;
    gp.registe(t3).registe(t4);
    gp.wait();

    std::cout << "about to yield 2\n";
    co_yield;
}

void callback2() {

    std::cout << "call2...\n";
    co_yield;

    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    TaskPtr t4 = std::make_shared<Task>(callback2);
//    globalTaskMgr.addRunnable(t4);

    std::cout << "call2......\n";
    co_yield;
}
 
void test() {
    TaskPtr t = std::make_shared<Task>(callback);
    TaskPtr t2 = std::make_shared<Task>(callback2);


    globalTaskMgr.addRunnable(t);
//    globalTaskMgr.addRunnable(t2);

    globalTaskMgr.debug_run();
}

int main() {
//    for ( int i = 0; i < 100000000; i++ )    test();
    test();
    std::cout << "ok...\n";
}
