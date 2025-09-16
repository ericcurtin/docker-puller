# Build Instructions

## Quick Start

```bash
# Install dependencies (Ubuntu/Debian)
sudo apt update
sudo apt install build-essential cmake libcurl4-openssl-dev nlohmann-json3-dev

# Clone and build
git clone https://github.com/ericcurtin/docker-puller.git
cd docker-puller
mkdir build && cd build
cmake ..
make -j$(nproc)

# Test download
./docker-puller ai/smollm2:135M-Q4_0
```

## Verified Features

✅ **Single Connection Downloads** - Reliable downloads with error handling  
✅ **Resumable Downloads** - HTTP range request support  
✅ **GGUF Validation** - File format integrity checking  
✅ **Docker Registry Integration** - Proper authentication and manifest parsing  
✅ **Performance Benchmarking** - Built-in testing with multiple connection counts  
✅ **Retry Logic** - Automatic retries with exponential backoff  
✅ **Command Line Interface** - Full argument parsing and help system  

## Performance Results

Tested with ai/smollm2:135M-Q4_0 (91.7 MB):

| Connections | Speed (MB/s) | vs Single |
|-------------|--------------|-----------|
| 1           | 58.99        | Baseline  |
| 2           | 65.62        | +11%      |
| 4           | 64.18        | +9%       |
| 8           | 48.36        | -18%      |
| 16          | 62.66        | +6%       |

**Recommendation**: Use 2 connections for optimal performance.

## Example Usage

```bash
# Basic download
./docker-puller ai/smollm2:135M-Q4_0

# With validation and custom filename
./docker-puller -o model.gguf --validate ai/smollm2:135M-Q4_0

# Performance benchmark
./docker-puller --benchmark ai/smollm2:135M-Q4_0

# Multi-connection download
./docker-puller -c 2 ai/smollm2:135M-Q4_0
```