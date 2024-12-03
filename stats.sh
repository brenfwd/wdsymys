#!/bin/bash

# Ensure a program is provided
if [ -z "$1" ]; then
    echo "Usage: $0 <program>"
    exit 1
fi

PROGRAM=$1

# Step 1: Run objdump and count loads and stores
echo "Counting loads and stores in the binary..."

LOADS=$(objdump -d "$PROGRAM" | grep -o 'ldr' | wc -l)
STORES=$(objdump -d "$PROGRAM" | grep -o 'str' | wc -l)

echo "Number of load instructions: $LOADS"
echo "Number of store instructions: $STORES"

# Step 2: Use perf to run the program and gather instruction count and execution time
echo "Running the program with perf..."

# Run the program with perf to count instructions and measure execution time
PERF_OUTPUT=$(perf stat -e instructions,cycles,task-clock,context-switches,cpu-migrations -x, $PROGRAM 2>&1)

# Extract the relevant performance metrics from the output
INSTRUCTIONS=$(echo "$PERF_OUTPUT" | grep 'instructions' | awk -F, '{print $1}' | tr -d '[:space:]')
CYCLES=$(echo "$PERF_OUTPUT" | grep 'cycles' | awk -F, '{print $1}' | tr -d '[:space:]')
EXECUTION_TIME=$(echo "$PERF_OUTPUT" | grep 'task-clock' | awk -F, '{print $1}' | tr -d '[:space:]')

# Step 3: Output results
echo "Number of instructions executed: $INSTRUCTIONS"
echo "Total execution time: $EXECUTION_TIME seconds"
echo "Cycles: $CYCLES"
