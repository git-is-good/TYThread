package main

import (
    "fmt"
    "time"
    "sync"
    "runtime"
)

func bench1(size int) {
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
    fmt.Println(sum)
}

const (
    size = 1000000

    size2 = 10000
    N = 5000000
)

func bench2() {
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
}

func main() {
    start := time.Now()
    bench1(size)
//    bench2()
    delta := time.Now().Sub(start)
    fmt.Printf("<bench1@go:size:%v> duration:%v\n", size, delta)
//    fmt.Printf("<bench2@go:size:%v:N:%v> duration:%v, %v/op\n", size2, N, delta, delta / N)
}
