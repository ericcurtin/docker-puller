#!/bin/bash

# Performance testing script for docker-puller
# Tests different numbers of concurrent connections

MODEL="ai/smollm2:135M-Q4_0"
OUTPUT_FILE="performance_test.gguf"
RESULTS_FILE="performance_results.txt"

echo "Docker Puller Performance Test" > $RESULTS_FILE
echo "Model: $MODEL" >> $RESULTS_FILE
echo "Date: $(date)" >> $RESULTS_FILE
echo "================================" >> $RESULTS_FILE

# Test different connection counts
for connections in 1 2 4 8; do
    echo "Testing with $connections connections..."
    echo "" >> $RESULTS_FILE
    echo "Connections: $connections" >> $RESULTS_FILE
    
    # Remove existing file if present
    rm -f $OUTPUT_FILE
    
    # Run the test in dry-run mode for safety in CI environment
    echo "Running dry-run test with $connections connections..."
    if ./docker-puller -c $connections -v -n $MODEL >> $RESULTS_FILE 2>&1; then
        echo "✓ Dry-run test with $connections connections completed successfully"
    else
        echo "✗ Dry-run test with $connections connections failed"
    fi
    
    echo "---" >> $RESULTS_FILE
done

echo "Performance testing completed. Results saved to $RESULTS_FILE"
cat $RESULTS_FILE