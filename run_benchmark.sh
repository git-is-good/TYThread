#!/bin/sh

make >/dev/null

echo "---- Start running YamiThread... ----"
./user_test
echo ""

echo "---- Start running Golang... ----"
go run benchmarks.go
echo ""

make omp_test >/dev/null
echo "---- Start running OpenMP... ----"
./omp_test
echo ""

rm omp_test

echo "Done..."
