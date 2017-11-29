#include <omp.h>
#include <stdio.h>

#include <assert.h>
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

const int N = 1600;
int A[N][N], B[N][N];
int C[N][N], C1[N][N];
int dense_mat_mut_test(void) {
    int i, j, k;
    double Start, End;
    
    srand((int)time(NULL));
    for (i = 0; i < N; ++i) {
        for (j = 0; j < N; ++j) {
            A[i][j] = rand() % 100;
            B[i][j] = rand() % 100;
        }
    }
//    printf("---- Serial\n");
//    Start = omp_get_wtime();
//    for (i = 0; i < N; ++i) {
//        for (j = 0; j < N; ++j) {
//            int Sum = 0;
//            for (k = 0; k < N; ++k) {
//                Sum += A[i][k] * B[k][j];
//            }
//            C[i][j] = Sum;
//        }
//    }
//    End = omp_get_wtime();
//    printf("---- Serial done in %f seconds.\n", End - Start);
    Start = omp_get_wtime();
#pragma omp parallel for collapse(2) private(k)
    for (i = 0; i < N; ++i) {
        for (j = 0; j < N; ++j) {
            int Sum = 0;
            for (k = 0; k < N; ++k) {
                Sum += A[i][k] * B[k][j];
            }
            C1[i][j] = Sum;
        }
    }
    End = omp_get_wtime();
    printf("<OpenMP:DenseMatMut> duration %fms.\n", (End - Start) * 1000);
//    printf("---- Check\n");
//    for (i = 0; i < N; i++) {
//        for (j = 0; j < N; j++) {
//            assert (C[i][j] == C1[i][j]);
//        }
//    }
//    printf("Passed\n");
    return 0;
}

int main() {
    dense_mat_mut_test();

//#pragma omp parallel
//    printf("hello from thread %d, nthreads %d\n", omp_get_thread_num(),
//            omp_get_num_threads());
}
