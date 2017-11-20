#include "Task.hh"
#include "TaskGroup.hh"
#include <iostream>
#include <utility>
#include <memory>
#include <functional>
#include <cassert>

void callback() {
    std::cout << "about to yield 1\n";
    co_yield;

    std::cout << "about to yield 2\n";
    co_yield;
}

void callback2() {

    std::cout << "call2...\n";
    co_yield;

    std::cout << "call2......\n";
    co_yield;
}
 
void test() {
    TaskPtr t = std::make_shared<Task>(callback);
    TaskPtr t2 = std::make_shared<Task>(callback2);

    TaskGroup gp;
    gp.registe(t).registe(t2);

    std::cout << "start...\n";
    bigTestTask = t;
    t->continuationIn();

    bigTestTask = t2;
    t2->continuationIn();

    while ( t->state != Task::Terminated ) {
        std::cout << "about to resume\n";
        bigTestTask = t;
        t->continuationIn();

        bigTestTask = t2;
        t2->continuationIn();
    }
    bigTestTask = nullptr;
    
    assert(t2->state == Task::Terminated);
    std::cout << "Done...\n";
}

int main() {
    test();
    std::cout << "test done...\n";
}
