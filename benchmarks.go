package main

import (
    "fmt"
    "time"
    "sync"
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

func main() {
    start := time.Now()
    bench1(1000000)
    delta := time.Now().Sub(start)
    fmt.Println("Duration:", delta)
}
