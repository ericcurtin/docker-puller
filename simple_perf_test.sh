#!/bin/bash

# Simple performance test for Docker AI Puller
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/build"
PULLER="$BUILD_DIR/docker-ai-puller"
TEST_MODEL="ai/smollm2:135M-Q4_0"

echo "Docker AI Puller Performance Test"
echo "================================="
echo "Model: $TEST_MODEL"
echo

# Test different connection counts
for connections in 1 2 4 8; do
    echo "Testing with $connections connections..."
    rm -f *.gguf
    
    start_time=$(date +%s.%N)
    if "$PULLER" -c "$connections" "$TEST_MODEL" > /dev/null 2>&1; then
        end_time=$(date +%s.%N)
        duration=$(echo "$end_time - $start_time" | bc -l)
        
        downloaded_file=$(find . -name "*.gguf" -type f | head -1)
        if [ -f "$downloaded_file" ]; then
            file_size=$(stat -c%s "$downloaded_file")
            file_size_mb=$(echo "scale=2; $file_size / 1024 / 1024" | bc -l)
            speed_mbps=$(echo "scale=2; $file_size_mb / $duration" | bc -l)
            
            printf "%d connections: %.1fs, %.1f MB, %.2f MB/s\n" "$connections" "$duration" "$file_size_mb" "$speed_mbps"
            rm -f "$downloaded_file"
        else
            echo "$connections connections: FAILED - No file created"
        fi
    else
        echo "$connections connections: FAILED"
    fi
done

echo
echo "Note: Docker registry doesn't support range requests,"
echo "so multi-connection downloads fall back to single connection."
echo "Performance differences are primarily due to overhead."