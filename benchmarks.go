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
    deltaf_nano := float32(delta / time.Nanosecond)
    deltaf := deltaf_nano / 1000000
    fmt.Printf("<Go  :massive_creation:goroutine:%-8d> duration: duration:%-9.3fms, %-7.3f ns/op\n", size, deltaf, deltaf_nano / float32(size))
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
    deltaf_nano := float32(delta / time.Nanosecond)
    deltaf := deltaf_nano / 1000000
    fmt.Printf("<Go  :massive_yield:goroutine:%-6d:total_yield:%v> duration:%-9.3fms, %-7.3f ns/op\n", size2, N, deltaf, deltaf_nano / N)
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
    fmt.Printf("<Go  :DenseMatMut:goroutine:%-4d> duration:%-9.3fms, %-7.3f ns/op\n", ntasks, delta)
}

var fake_sum int

func complex_scheduling_test(test_size int, num int) {
    var biggroup sync.WaitGroup
    biggroup.Add(test_size)
    start := time.Now()

    for count := 0; count < test_size; count++ {
        go func () {
            var group sync.WaitGroup
            group.Add(num)

            for i := 0; i < num; i++ {
                go func() {
                    fake_sum += 57
                    group.Done()
                } ()
            }
            group.Wait()
            biggroup.Done()
        } ()
    }
    biggroup.Wait()
    delta := time.Now().Sub(start)
    deltaf_nano := float32(delta / time.Nanosecond)
    deltaf := deltaf_nano / 1000000
    fmt.Printf("<Go  :complex_scheduling:%-6dx%-6d> duration:%-9.3fms, %-7.3f ns/op\n",
            test_size, num, deltaf, deltaf_nano / float32(test_size * num))
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

    complex_scheduling_test(100, 100000)
    complex_scheduling_test(1000, 10000)
    complex_scheduling_test(10000, 1000)
    complex_scheduling_test(100000, 100)

//    dense_mat_mut_test(4)
//    dense_mat_mut_test(40)
//    dense_mat_mut_test(400)
//    dense_mat_mut_test(800)

}
