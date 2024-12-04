#!/bin/bash

set -euo pipefail

# Ensure a program is provided
if [ -z "$1" ]; then
    echo "Usage: $0 <program>"
    exit 1
fi

./run $1
PROGRAM=build/testcases/$1

profile() {
    # Step 1: Run objdump and count loads and stores
    LOADS=$(objdump -d "$1" | grep -E '\b(ldr|ldrb|ldrh|ldrsb|ldrsh|ldrd|ldur|ldp)\b' | wc -l)
    STORES=$(objdump -d "$1" | grep -E '\b(str|strb|strh|strsb|strsh|strd|stur|stp|stld)\b' | wc -l)

    cat $1.su

    echo "Number of load instructions: $LOADS"
    echo "Number of store instructions: $STORES"

    if [[ "$OSTYPE" == "linux-gnu"* ]]; then
        # Step 2: Use perf to run the program and gather instruction count and execution time
        # Run the program with perf to count instructions and measure execution time
        PERF_OUTPUT=$(perf stat -e instructions,cycles,task-clock,context-switches,cpu-migrations -x, $1 2>&1)

        # Extract the relevant performance metrics from the output
        INSTRUCTIONS=$(echo "$PERF_OUTPUT" | grep 'instructions' | awk -F, '{print $1}' | tr -d '[:space:]')
        CYCLES=$(echo "$PERF_OUTPUT" | grep 'cycles' | awk -F, '{print $1}' | tr -d '[:space:]')
        EXECUTION_TIME=$(echo "$PERF_OUTPUT" | grep 'task-clock' | awk -F, '{print $1}' | tr -d '[:space:]')

        # Step 3: Output results
        echo "Number of instructions executed: $INSTRUCTIONS"
        echo "Total execution time: $EXECUTION_TIME seconds"
        echo "Cycles: $CYCLES"
    elif [[ "$OSTYPE" == "darwin"* ]]; then
        time $1
    else
        echo "L + unsupported OS"
    fi
}

echo "---- WDSYMYS ----"
profile $PROGRAM

echo "---- BASELINE ----"
profile $PROGRAM.baseline