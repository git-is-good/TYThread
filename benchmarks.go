package main

import (
    "fmt"
    "time"
    "sync"
    "runtime"
    "math/rand"
)

func massive_creation_test(size int) {
    start := time.Now()
    var group sync.WaitGroup
    group.Add(size)
    sum := 0

    for i := 0; i < size; i++ {
        go func() {
            sum += 1
            group.Done()
        } ()
    }
    group.Wait()
    delta := time.Now().Sub(start)
    fmt.Printf("<Go  :massive_creation:goroutine:%-8d> duration:%v, %v/op\n", size, delta, delta / time.Duration(size))
}

const (
    N = 5000000
)

func massive_yield_test(size2 int) {
    start := time.Now()
    var group sync.WaitGroup
    group.Add(size2)

    for i := 0; i < size2; i++ {
        go func () {
            for j := 0; j < N / size2; j++ {
                runtime.Gosched()
            }
            group.Done()
        } ()
    }
    group.Wait()
    delta := time.Now().Sub(start)
    fmt.Printf("<Go  :massive_yield:goroutine:%-6d:total_yield:%v> duration:%v, %v/op\n", size2, N, delta, delta / N)
}

const M = 1600
var A [M][M]int
var B [M][M]int
var C1 [M][M]int
var C2 [M][M]int

func dense_mat_mut_test(ntasks int) {
    for i := 0; i < M; i++ {
        for j := 0; j < M; j++ {
            A[i][j] = rand.Int() % 100
            B[i][j] = rand.Int() % 100
        }
    }

    if M % ntasks != 0 {
        panic("ntasks must divide M in benchmark")
    }

    chunk := M / ntasks

    start := time.Now()
    var group sync.WaitGroup
    group.Add(ntasks)

    for i := 0; i < ntasks; i++ {
        go func (i int) {
            for s := 0; s < chunk; s++ {
                r := i * chunk + s
                for j := 0; j < M; j++ {
                    sum := 0
                    for k := 0; k < M; k++ {
                        sum += A[r][k] * B[k][j]
                    }
                    C2[r][j] = sum
                }
            }
            group.Done()
        } (i)
    }
    group.Wait()
    delta := time.Now().Sub(start)
    fmt.Printf("<Go  :DenseMatMut:goroutine:%-4d> duration:%v\n", ntasks, delta)
}

func complex_scheduling_test(num int) {
    from := int64(0)
    gap := int64(1)
    test_size := 10000

    var biggroup sync.WaitGroup
    biggroup.Add(test_size)
    start := time.Now()

    for count := 0; count < test_size; count++ {
        go func () {
            var group sync.WaitGroup
            group.Add(num)

            ress := make([]int64, num)

            for i := 0; i < num; i++ {
                go func(i int) {
                    for k := from + gap * int64(i); k < from + gap * (int64(i) + 1); k++ {
                        ress[i] += k
                    }
                    group.Done()
                } (i)
            }
            group.Wait()
            biggroup.Done()
        } ()
    }
    biggroup.Wait()
    delta := time.Now().Sub(start)
    fmt.Printf("<Go  :complex_task_scheduling:%-4dx%-4d> duration:%v\n",
            test_size, num, delta)
}


func main() {
    massive_yield_test(100)
    massive_yield_test(1000)
    massive_yield_test(10000)
    massive_yield_test(100000)

    massive_creation_test(10000)
    massive_creation_test(100000)
    massive_creation_test(1000000)
    massive_creation_test(10000000)

    complex_scheduling_test(1000)

//    dense_mat_mut_test(4)
//    dense_mat_mut_test(40)
//    dense_mat_mut_test(400)
//    dense_mat_mut_test(800)

}
