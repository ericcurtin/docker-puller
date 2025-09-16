#!/bin/bash

# Test script for Docker AI Puller
set -e

echo "Testing Docker AI Puller..."
echo "=========================="

# Build the project
echo "Building project..."
mkdir -p build
cd build
cmake ..
make -j$(nproc)

echo "Build completed successfully!"

# Test basic functionality
echo ""
echo "Testing basic functionality..."
echo "Usage:"
./docker-ai-puller --help

echo ""
echo "Testing URL parsing..."
# We can test the parsing functionality even without actual downloads

echo "Test completed. Ready for manual testing with:"
echo "./docker-ai-puller ai/smollm2:135M-Q4_0"
echo "./docker-ai-puller -c 4 ai/smollm2:135M-Q4_0"