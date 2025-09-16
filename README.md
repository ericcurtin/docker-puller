# Docker AI Model Puller

A high-performance C++ tool for downloading Docker AI models in GGUF format with multi-connection acceleration and resumable downloads.

## Features

- **Multi-connection downloads**: Use multiple parallel connections to accelerate downloads (1-16 connections)
- **Resumable downloads**: Automatically resume interrupted downloads
- **Automatic retries**: 3 automatic retry attempts on failure
- **GGUF validation**: Validates downloaded files to ensure integrity
- **Performance testing**: Built-in performance testing to find optimal connection count

## Building

### Prerequisites

- CMake 3.16 or higher
- C++17 compatible compiler
- libcurl development headers

### Ubuntu/Debian

```bash
sudo apt update
sudo apt install -y build-essential cmake libcurl4-openssl-dev pkg-config
```

### Build

```bash
mkdir build
cd build
cmake ..
make
```

## Usage

### Basic Usage

```bash
# Download a model with default settings (1 connection)
./docker-ai-puller ai/smollm2:135M-Q4_0

# Use 4 parallel connections for faster download
./docker-ai-puller -c 4 ai/smollm2:135M-Q4_0

# Specify custom output path
./docker-ai-puller -c 8 -o my_model.gguf ai/smollm2:135M-Q4_0

# Set custom retry count
./docker-ai-puller -c 4 -r 5 ai/smollm2:135M-Q4_0
```

### Performance Testing

```bash
# Run performance tests to find optimal connection count
./docker-ai-puller --test
```

## Command Line Options

| Option | Description | Default |
|--------|-------------|---------|
| `-c, --connections NUM` | Number of concurrent connections (1-16) | 1 |
| `-r, --retries NUM` | Number of retry attempts (0-10) | 3 |
| `-o, --output PATH` | Output file path | auto-generated |
| `-t, --test` | Run performance tests | - |
| `-h, --help` | Show help message | - |

## Performance Optimization

Based on performance testing, the optimal number of connections depends on several factors:

### Network Bandwidth
- **Low bandwidth (< 10 Mbps)**: 1-2 connections recommended
- **Medium bandwidth (10-100 Mbps)**: 4-8 connections recommended  
- **High bandwidth (> 100 Mbps)**: 8-16 connections recommended

### File Size
- **Small files (< 100 MB)**: Overhead of multiple connections may not be worth it, use 1-2 connections
- **Large files (> 1 GB)**: Multiple connections provide significant benefits, use 8-16 connections

### Server Limitations
- Some servers may rate-limit or throttle multiple connections from the same IP
- If downloads fail with many connections, reduce the count

### Recommended Starting Points

For most use cases, we recommend starting with:
- **4 connections** for typical broadband connections
- **8 connections** for high-speed connections
- **1 connection** if experiencing connection issues

Run `./docker-ai-puller --test` to determine the optimal setting for your specific environment.

## Examples

### Download Examples

```bash
# Basic download
./docker-ai-puller ai/smollm2:135M-Q4_0

# Fast download with 8 connections
./docker-ai-puller -c 8 ai/smollm2:135M-Q4_0

# Download with custom filename
./docker-ai-puller -c 4 -o my-smoll-model.gguf ai/smollm2:135M-Q4_0

# Conservative download with extra retries
./docker-ai-puller -c 2 -r 5 ai/smollm2:135M-Q4_0
```

### Performance Testing

```bash
# Run full performance test suite
./docker-ai-puller --test

# Expected output:
# Running performance tests with model: ai/smollm2:135M-Q4_0
# Testing connection counts: 1 2 4 8 16
# 
# Testing with 1 connection(s)...
#   Time: 45.32s, Throughput: 2.87 MB/s, Validation: PASS
# Testing with 2 connection(s)...
#   Time: 24.15s, Throughput: 5.38 MB/s, Validation: PASS
# ...
# 
# Recommended optimal connection count: 8
```

## Technical Details

### Multi-Connection Implementation

The tool uses libcurl's multi interface to download different byte ranges of the file simultaneously:

1. **Range Calculation**: File is divided into equal chunks based on connection count
2. **Parallel Downloads**: Each connection downloads its assigned byte range
3. **File Assembly**: Downloaded chunks are written to the correct file positions
4. **Thread Safety**: File writes are synchronized using mutexes

### Resume Mechanism

- Checks existing file size on startup
- Compares with expected total size from server
- Resumes download from current position if partial file exists
- Validates partial files before resuming

### GGUF Validation

The tool validates downloaded files using the GGUF format specification:

- **Magic Number**: Verifies 0x46554747 ("GGUF")
- **Version**: Supports version 3
- **Header Structure**: Validates tensor count and metadata
- **Metadata Parsing**: Basic validation of key-value pairs

## Error Handling

- **Network Errors**: Automatic retry with exponential backoff
- **File System Errors**: Clear error messages for disk space, permissions
- **Validation Errors**: Re-downloads invalid files
- **Server Errors**: Handles HTTP error codes gracefully

## License

Licensed under the Apache License, Version 2.0. See LICENSE file for details.

## Contributing

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Add tests if applicable
5. Submit a pull request

## Troubleshooting

### Common Issues

**Downloads fail with multiple connections:**
- Try reducing connection count: `-c 2`
- Some servers limit concurrent connections

**File validation fails:**
- File may be corrupted during download
- Tool will automatically retry and re-download

**Slow download speeds:**
- Run performance tests: `--test`
- Network or server may be limiting bandwidth
- Try different connection counts

**Build errors:**
- Ensure libcurl development headers are installed
- Check CMake and compiler versions meet requirements