#!/bin/bash

# Create a test file to demonstrate resumable downloads
# This simulates what would happen with a real download

echo "Creating Test File for Resume Demonstration"
echo "==========================================="

# Create a test file
echo "Creating 1MB test file..."
dd if=/dev/zero of=test_model.gguf bs=1024 count=1024 2>/dev/null

echo "Original file size: $(stat -c%s test_model.gguf) bytes"

# Simulate partial download by truncating the file
echo "Simulating partial download (keeping first 512KB)..."
dd if=test_model.gguf of=test_model_partial.gguf bs=1024 count=512 2>/dev/null

echo "Partial file size: $(stat -c%s test_model_partial.gguf) bytes"

# Show file sizes
echo ""
echo "Files created:"
echo "  test_model.gguf (complete): $(stat -c%s test_model.gguf) bytes"
echo "  test_model_partial.gguf (partial): $(stat -c%s test_model_partial.gguf) bytes"
echo ""

echo "This demonstrates the file size detection and range request setup"
echo "that would be used for resumable downloads in a real scenario."
echo ""

# Cleanup
rm -f test_model.gguf test_model_partial.gguf

echo "Demonstration complete!"