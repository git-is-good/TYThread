#ifndef _MPI_HOOKS_HH_
#define _MPI_HOOKS_HH_

#include <mpi.h>

int MPI_Send_Hook(const void *buf, int count, MPI_Datatype datatype,
        int dest, int tag, MPI_Comm comm);

int MPI_Recv_Hook(void *buf, int count, MPI_Datatype datatype,
        int source, int tag, MPI_Comm comm, MPI_Status *status);

#endif /* _MPI_HOOKS_HH_ */

