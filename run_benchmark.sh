#!/bin/sh

make >/dev/null

echo "---- Start running YamiThread... ----"
./user_test
echo

echo "---- Start running Golang... ----"
go run benchmarks.go
echo

make omp_test >/dev/null
make omp_merge_sort >/dev/null
echo "---- Start running OpenMP... ----"
./omp_test
./omp_merge_sort 1000000000 6
echo
rm omp_test
rm omp_merge_sort

echo "Done..."
