#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <thread>
#include <chrono>
#include <vector>
#include <string>

#include "co_user.hh"

#define CHECKRET(r)                             \
    if ( (r) != MPI_SUCCESS ) {                 \
        printf("%d:%s: Error: %d\n",            \
                __LINE__, __func__, r);         \
        MPI_Abort(MPI_COMM_WORLD, -1);          \
    }

int mpi_comm_size;
int mpi_me;

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
        printf("<%s> duration: %lfms\n", message.c_str(), 
                std::chrono::duration<double, std::milli>(period).count());
    }
};

void process0() {
//    std::thread tid([] () {
//        // this evil thread will acquire the lock
//        // MPI_Send will never succeed
//        char data[10];
//        int r;
//
//      r = MPI_Ssend(data, sizeof(data), MPI_CHAR,
//              1, 10, MPI_COMM_WORLD);
//      CHECKRET(r);
//    });


    /* emulate a huge amount of computation,
     * this will block the coroutine worker
     */
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    printf("process0 after sleep\n");
    char real_data[10];
    int r;
    {
        TimeInterval _("MPI_Ssend 0");
        r = MPI_Ssend(real_data, sizeof(real_data), MPI_CHAR,
                1, 100, MPI_COMM_WORLD);
    }

    CHECKRET(r);
    printf("OK, no global lock: %d\n", mpi_me);
//    co_terminate();
//    tid.join();
}

void process1() {
    char real_data[10];
    int r;
//    std::this_thread::sleep_for(std::chrono::milliseconds(1500));

    for ( int i = 0; i < 10; i++ ) {
//        go([i] () {
//                printf("Running: I am a huge task %d at process %d\n", i, mpi_me);
//        });
    }

    {
        TimeInterval _("MPI_Recv 1");
        
//        MPI_Request request;
//        MPI_Recv_Hook(real_data, sizeof(real_data), MPI_CHAR,
//                0, 100, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
      r = MPI_Recv(real_data, sizeof(real_data), MPI_CHAR,
              0, 100, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
//      CHECKRET(r);

//        r = MPI_Irecv(real_data, sizeof(real_data), MPI_CHAR,
//                0, 100, MPI_COMM_WORLD, &request);
//        CHECKRET(r);
//
//        int finished = 0;
//        while ( !finished ) {
//            r = MPI_Test(&request, &finished, MPI_STATUS_IGNORE);
//            CHECKRET(r);
//        }
    }

    printf("OK, no global lock: %d\n", mpi_me);
//    co_terminate();
}

int main() {
    int thread_level;

    MPI_Init_thread(NULL, NULL, MPI_THREAD_MULTIPLE, &thread_level);

    if ( thread_level != MPI_THREAD_MULTIPLE ) {
        printf("thread level provided: %d\n", thread_level - MPI_THREAD_SINGLE);
        MPI_Abort(MPI_COMM_WORLD, -1);
    }

    MPI_Comm_rank(MPI_COMM_WORLD, &mpi_me);
    MPI_Comm_size(MPI_COMM_WORLD, &mpi_comm_size);
    printf("hello from %d of %d\n", mpi_me, mpi_comm_size);

//    co_init();

    if ( mpi_me == 0 ) {
//        go(process0);
        process0();
    } else if ( mpi_me == 1 ) {
//        go(process1);
        process1();
    }
    
//    co_mainloop();
    
    printf("should print.\n");

    MPI_Finalize();
}
