#include "GlobalMediator.hh"
#include "Task.hh"
#include "TaskGroup.hh"
#include "util.hh"

#include <memory>
#include <stdio.h>
#include <thread>
#include <chrono>

const int matsz = 10000;

void co_main2() {
    std::vector<int> mat(matsz);


    
    



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
    for ( int i = 0; i < 1000000; i++ ) co_main1(i);
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
