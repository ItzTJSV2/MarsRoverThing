#!/bin/bash

echo "[BUILDING]"
gcc -O2 mm_bench.c allocator.c -o mm_bench

if [ ! -f mm_bench ]; then
    echo "Build failed."
    exit 1
fi

echo ""
echo "[RUNNING BENCHMARK]"

# Use shell builtin 'time'
time ./mm_bench
