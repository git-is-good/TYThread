#ifndef _MPI_HOOKS_CC_
#define _MPI_HOOKS_CC_

#include <mpi.h>
#include <Task.hh>

#define RETURN_IF_NOT_SUCCESS(r) \
    if ( (r) != MPI_SUCCESS ) return;

int
MPI_Send_Hook(
        const void *buf, int count, MPI_Datatype datatype,
        int dest, int tag, MPI_Comm comm)
{
    MPI_Request request;
    int r;
    r = MPI_Isend(buf, count, datatype, dest, tag, comm, &request);
    RETURN_IF_NOT_SUCCESS(r);

    int flag;
    r = MPI_Test(&request, &flag, MPI_STATUS_IGNORE);
    RETURN_IF_NOT_SUCCESS(r);
    if ( flag ) {
        // already ok
        return r;
    }

    co_currentTask->state = Task::MPIBlocked;
    co_yield;

    for ( ;; ) {
        r = MPI_Test(&request, &flag, MPI_STATUS_IGNORE);
        RETURN_IF_NOT_SUCCESS(r);
        if ( flag ) {
            // done
            co_currentTask->state = Task::Runnable;
            return r;
        }

        co_yield;
    }
}

int
MPI_Recv_Hook(
        void *buf, int count, MPI_Datatype datatype,
        int source, int tag, MPI_Comm comm, MPI_Status *status)
{
    MPI_Request request;
    int r;

    r = MPI_Irecv(buf, count, datatype, source, tag, comm, &request);
    RETURN_IF_NOT_SUCCESS(r);

    int flag;
    r = MPI_Test(&request, &flag, MPI_STATUS_IGNORE);
    RETURN_IF_NOT_SUCCESS(r);
    if ( flag ) {
        // already ok
        return r;
    }

    co_currentTask->state = Task::MPIBlocked;
    co_yield;

    for ( ;; ) {
        r = MPI_Test(&request, &flag, status);
        RETURN_IF_NOT_SUCCESS(r);
    }

    if ( flag ) {
        // done
        co_currentTask->state = Task::Runnable;
        return r;
    }

    co_yield;
}

#endif /* _MPI_HOOKS_CC_ */

