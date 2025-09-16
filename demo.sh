#!/bin/bash

# Demo script to show docker-puller functionality
# This script demonstrates the tool with various configurations

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/build"
DOCKER_PULLER="$BUILD_DIR/docker-puller"

echo "Docker AI Model Puller - Demonstration"
echo "======================================="
echo ""

# Check if docker-puller exists
if [ ! -f "$DOCKER_PULLER" ]; then
    echo "Error: docker-puller not found. Building..."
    mkdir -p "$BUILD_DIR"
    cd "$BUILD_DIR"
    cmake .. && make
    cd "$SCRIPT_DIR"
fi

echo "1. Showing help information:"
echo "----------------------------"
"$DOCKER_PULLER" --help
echo ""

echo "2. Showing version information:"
echo "-------------------------------"
"$DOCKER_PULLER" --version
echo ""

echo "3. Testing invalid model specification:"
echo "---------------------------------------"
"$DOCKER_PULLER" invalid-spec 2>&1 || true
echo ""

echo "4. Testing valid model specification (will attempt download):"
echo "-------------------------------------------------------------"
echo "Note: This would normally download from Docker registry"
echo "Command: $DOCKER_PULLER ai/smollm2:135M-Q4_0"
echo ""
timeout 5 "$DOCKER_PULLER" ai/smollm2:135M-Q4_0 2>&1 || echo "Command completed/timed out"
echo ""

echo "5. Testing with multiple connections:"
echo "-------------------------------------"
echo "Command: $DOCKER_PULLER -c 4 ai/smollm2:135M-Q4_0"
echo ""
timeout 5 "$DOCKER_PULLER" -c 4 ai/smollm2:135M-Q4_0 demo-output.gguf 2>&1 || echo "Command completed/timed out"
echo ""

echo "Demonstration completed!"
echo ""
echo "In a real environment with actual Docker AI models:"
echo "- The tool would connect to Docker registries"
echo "- Download large GGUF model files (GB in size)"
echo "- Use multiple connections for faster downloads"
echo "- Support resume for interrupted downloads"
echo "- Automatically retry on failures"

# Clean up any test files
rm -f smollm2_135M-Q4_0.gguf demo-output.gguf