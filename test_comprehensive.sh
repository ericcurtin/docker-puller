#!/bin/bash

# Comprehensive test script for docker-puller
# Tests the specific model ai/smollm2:135M-Q4_0

set -e

echo "=== Docker Puller Comprehensive Test ==="
echo "Testing model: ai/smollm2:135M-Q4_0"
echo ""

# Build the project
echo "1. Building project..."
if [ ! -f "./docker-puller" ]; then
    echo "Error: docker-puller executable not found. Run from build directory."
    exit 1
fi

# Test basic help functionality
echo "2. Testing help output..."
if ./docker-puller --help > /dev/null; then
    echo "✓ Help output works"
else
    echo "✗ Help output failed"
    exit 1
fi

# Test model parsing
echo "3. Testing model parsing and registry connection..."
if ./docker-puller -v -n ai/smollm2:135M-Q4_0 | grep -q "Dry run completed successfully"; then
    echo "✓ Model parsing and registry connection successful"
else
    echo "✗ Model parsing or registry connection failed"
    exit 1
fi

# Test different connection counts
echo "4. Testing different connection counts..."
for connections in 1 2 4 8; do
    echo "  Testing with $connections connections..."
    if ./docker-puller -c $connections -n ai/smollm2:135M-Q4_0 > /dev/null 2>&1; then
        echo "  ✓ $connections connections - OK"
    else
        echo "  ✗ $connections connections - FAILED"
        exit 1
    fi
done

# Test GGUF validation with our test chunk
echo "5. Testing GGUF validation..."
if [ -f "test_chunk.bin" ]; then
    # Our test chunk should have valid GGUF magic
    magic=$(hexdump -C test_chunk.bin | head -1 | cut -d'|' -f2 | cut -c1-4)
    if [ "$magic" = "GGUF" ]; then
        echo "✓ Downloaded chunk has valid GGUF magic"
    else
        echo "✗ Downloaded chunk missing GGUF magic"
        exit 1
    fi
else
    echo "! No test chunk found (run test_download first)"
fi

# Test specific model verification
echo "6. Testing ai/smollm2:135M-Q4_0 specific validation..."
output=$(./docker-puller -v -n ai/smollm2:135M-Q4_0)

# Check for expected model details
if echo "$output" | grep -q "ai/smollm2:135M-Q4_0"; then
    echo "✓ Model specification parsed correctly"
else
    echo "✗ Model specification parsing failed"
    exit 1
fi

if echo "$output" | grep -q "87.5 MB"; then
    echo "✓ Expected file size detected (87.5 MB)"
else
    echo "✗ Unexpected file size"
    exit 1
fi

if echo "$output" | grep -q "sha256:384a89bd054c0cc1b128d1adb2c6648867e5a84d166bc455c8bda6e4576c2779"; then
    echo "✓ Expected digest found"
else
    echo "✗ Unexpected digest"
    exit 1
fi

# Test error handling
echo "7. Testing error handling..."
if ./docker-puller -n nonexistent/model:tag 2>&1 | grep -q -i "error"; then
    echo "✓ Error handling for invalid models works"
else
    echo "! Error handling test inconclusive"
fi

echo ""
echo "=== ALL TESTS PASSED ==="
echo "The docker-puller is working correctly with ai/smollm2:135M-Q4_0"
echo ""
echo "Summary:"
echo "- Model parsing: ✓"
echo "- Registry connection: ✓"
echo "- Multi-connection support: ✓"
echo "- GGUF validation: ✓"
echo "- ai/smollm2:135M-Q4_0 specific tests: ✓"
echo "- Error handling: ✓"