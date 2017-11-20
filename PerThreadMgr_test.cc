#include "PerThreadMgr.hh"
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

    globalTaskMgr.addRunnable(t3);

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
    globalTaskMgr.addRunnable(t2);

    globalTaskMgr.debug_run();
}

int main() {
    test();
    std::cout << "ok...\n";
}
