#!/bin/bash

# Performance testing script for Docker AI Puller
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/build"
PULLER="$BUILD_DIR/docker-ai-puller"
TEST_MODEL="ai/smollm2:135M-Q4_0"
OUTPUT_DIR="$SCRIPT_DIR/test_output"
RESULTS_FILE="$SCRIPT_DIR/performance_results.txt"

echo "Docker AI Puller Performance Testing"
echo "===================================="
echo "Model: $TEST_MODEL"
echo "Output directory: $OUTPUT_DIR"
echo

# Ensure the project is built
if [ ! -f "$PULLER" ]; then
    echo "Building project first..."
    cd "$BUILD_DIR"
    make -j$(nproc)
    cd "$SCRIPT_DIR"
fi

# Create output directory
mkdir -p "$OUTPUT_DIR"

# Initialize results file
echo "Docker AI Puller Performance Test Results" > "$RESULTS_FILE"
echo "Model: $TEST_MODEL" >> "$RESULTS_FILE"
echo "Date: $(date)" >> "$RESULTS_FILE"
echo "System: $(uname -a)" >> "$RESULTS_FILE"
echo "CPU: $(nproc) cores" >> "$RESULTS_FILE"
echo >> "$RESULTS_FILE"

# Test different connection counts
CONNECTION_COUNTS=(1 2 4 8)

echo "Starting performance tests..."
echo "Connections | Download Time | File Size | Speed (MB/s) | Status"
echo "------------|---------------|-----------|--------------|--------"

for connections in "${CONNECTION_COUNTS[@]}"; do
    echo -n "     $connections      |"
    
    # Clean up previous download
    output_file="$OUTPUT_DIR/test_${connections}_connections.gguf"
    rm -f "$output_file"
    
    # Measure download time
    start_time=$(date +%s.%N)
    
    if timeout 300 "$PULLER" -c "$connections" -o "$OUTPUT_DIR" "$TEST_MODEL" > /dev/null 2>&1; then
        end_time=$(date +%s.%N)
        duration=$(echo "$end_time - $start_time" | bc -l)
        
        # Check if file was created (might have different name)
        downloaded_file=$(find "$OUTPUT_DIR" -name "*.gguf" -type f | head -1)
        
        if [ -f "$downloaded_file" ]; then
            file_size=$(stat -c%s "$downloaded_file")
            file_size_mb=$(echo "scale=2; $file_size / 1024 / 1024" | bc -l)
            speed_mbps=$(echo "scale=2; $file_size_mb / $duration" | bc -l)
            
            printf "%8.1fs |%8.1f MB |%9.2f MB/s | SUCCESS\n" "$duration" "$file_size_mb" "$speed_mbps"
            
            # Log to results file
            echo "$connections connections: ${duration}s, ${file_size_mb}MB, ${speed_mbps}MB/s - SUCCESS" >> "$RESULTS_FILE"
            
            # Clean up after successful test
            rm -f "$downloaded_file"
        else
            echo "       N/A |        N/A |          N/A | NO FILE"
            echo "$connections connections: FAILED - No file created" >> "$RESULTS_FILE"
        fi
    else
        echo "     TIMEOUT |        N/A |          N/A | TIMEOUT"
        echo "$connections connections: TIMEOUT (>300s)" >> "$RESULTS_FILE"
    fi
done

echo
echo "Performance test completed. Results saved to: $RESULTS_FILE"
echo
echo "Summary from results file:"
echo "========================="
cat "$RESULTS_FILE"

# Clean up test directory
rm -rf "$OUTPUT_DIR"

echo
echo "Test recommendations:"
echo "- For small models (< 100MB): 1-2 connections recommended"
echo "- For medium models (100MB-1GB): 2-4 connections recommended"  
echo "- For large models (> 1GB): 4-8 connections recommended"
echo "- Optimal connection count depends on network bandwidth and server limits"