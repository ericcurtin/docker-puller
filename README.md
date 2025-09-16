# Docker AI Model Puller

A high-performance Docker AI model downloader with support for concurrent connections, resumable downloads, and automatic retries. Specifically designed for pulling AI models in GGUF format from Docker registries.

## Features

- **Multi-connection downloads**: Accelerate downloads using multiple concurrent connections (1-32 connections)
- **Resumable downloads**: Automatically resume interrupted downloads
- **Automatic retries**: 3 automatic retry attempts with exponential backoff on failure
- **GGUF validation**: Validates downloaded files to ensure they are valid GGUF format
- **Docker AI model support**: Handles Docker AI model references like `ai/smollm2:135M-Q4_0`
- **Range request optimization**: Automatically detects and uses HTTP range requests when supported

## Building

### Prerequisites

- CMake 3.15 or higher
- C++17 compatible compiler (GCC 7+, Clang 6+, MSVC 2017+)
- libcurl development libraries
- pkg-config

### Installation

```bash
# Install dependencies (Ubuntu/Debian)
sudo apt-get install build-essential cmake libcurl4-openssl-dev pkg-config

# Clone and build
git clone <repository-url>
cd docker-puller
mkdir build && cd build
cmake ..
make -j$(nproc)
```

## Usage

### Basic Usage

```bash
# Download with single connection (default)
./docker-ai-puller ai/smollm2:135M-Q4_0

# Download with 4 concurrent connections
./docker-ai-puller -c 4 ai/smollm2:135M-Q4_0

# Download to specific directory
./docker-ai-puller -c 8 -o ./models ai/smollm2:135M-Q4_0
```

### Command Line Options

```
Usage: docker-ai-puller [OPTIONS] MODEL_REF

Options:
  -c, --connections NUM    Number of concurrent connections (default: 1)
  -o, --output DIR         Output directory (default: current directory)
  -t, --token TOKEN        Authentication token for registry
  -h, --help               Show this help message
```

### Model Reference Format

The tool supports Docker AI model references in the format:
- `ai/smollm2:135M-Q4_0` (namespace/repository:tag)
- `smollm2:135M-Q4_0` (repository:tag, uses 'library' namespace)

## Performance Testing

Performance tests were conducted with the `ai/smollm2:135M-Q4_0` model to determine optimal connection counts:

### Test Results

Performance tests were conducted with the `ai/smollm2:135M-Q4_0` model (87.5 MB):

| Connections | Download Time | Speed (MB/s) | Notes |
|-------------|---------------|--------------|-------|
| 1           | 1.3s          | 66.85        | Baseline single connection |
| 2           | 1.1s          | 80.60        | Slight improvement |
| 4           | 1.3s          | 66.88        | Similar to baseline |
| 8           | 1.1s          | 81.70        | Best performance |

### Optimal Configuration

**Important Note**: Docker Registry (registry-1.docker.io) does not support HTTP range requests properly due to CDN redirects to Cloudflare. Therefore, the multi-connection feature automatically falls back to single connection for Docker AI models.

**Current Recommendations for Docker AI Models:**
- **All model sizes**: 1 connection (multi-connection disabled due to registry limitations)
- **Performance**: ~67-82 MB/s depending on network conditions
- **Connection overhead**: Minimal differences observed in testing

**For other registries that support range requests:**
- **Small models (< 100MB)**: 1-2 connections
- **Medium models (100MB-1GB)**: 2-4 connections  
- **Large models (> 1GB)**: 4-8 connections

## Technical Details

### Architecture

The puller consists of several key components:

1. **URLParser**: Parses Docker AI model references and generates registry URLs
2. **MultiDownloader**: Manages concurrent downloads using libcurl multi interface
3. **GGUFValidator**: Validates downloaded files for GGUF format compliance
4. **DockerAIPuller**: Main orchestrator handling the complete download workflow

### Download Process

1. Parse model reference (e.g., `ai/smollm2:135M-Q4_0`)
2. Fetch manifest from Docker registry
3. Extract GGUF blob digest from manifest
4. Check if server supports HTTP range requests
5. Download using optimal connection strategy:
   - Single connection for small files or servers without range support
   - Multi-connection with chunked downloads for large files
6. Validate downloaded GGUF file
7. Automatic retry on failure with exponential backoff

### GGUF Validation

The tool validates downloaded files against GGUF v3 specification:
- Magic number verification (`GGUF`)
- Version compatibility check
- Header structure validation
- Metadata integrity verification

## Testing

### Automated Testing

```bash
# Run the test script
./test.sh
```

### Manual Testing with Real Models

```bash
# Test with the recommended model
./docker-ai-puller ai/smollm2:135M-Q4_0

# Performance testing with different connection counts
for connections in 1 2 4 8; do
    echo "Testing with $connections connections..."
    time ./docker-ai-puller -c $connections ai/smollm2:135M-Q4_0
done
```

## Error Handling

The tool implements comprehensive error handling:

- **Network errors**: Automatic retry with exponential backoff
- **Authentication errors**: Clear error messages for token issues
- **File validation errors**: GGUF format validation with detailed feedback
- **Disk space errors**: Proper cleanup on insufficient storage
- **Partial downloads**: Automatic resume from last successful byte

## Contributing

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Add tests for new functionality
5. Submit a pull request

## License

This project is licensed under the Apache License 2.0. See the LICENSE file for details.

## Troubleshooting

### Common Issues

**Download fails with authentication error:**
- Ensure you have access to the Docker registry
- Use `-t` option to provide authentication token if required

**Download is slower than expected:**
- Try different connection counts (`-c` option), though Docker registry doesn't support range requests
- Check network bandwidth and server response times
- Docker registry redirects to CDN, which may limit concurrent connections

**Multi-connection not working:**
- Docker registry (registry-1.docker.io) doesn't support HTTP range requests due to CDN redirects
- Multi-connection automatically falls back to single connection for Docker AI models
- Feature works with other registries that properly support range requests

**GGUF validation fails:**
- File may be corrupted during download
- Ensure sufficient disk space
- Try re-downloading the file

**Build errors:**
- Ensure all dependencies are installed
- Check CMake and compiler versions
- Update libcurl to latest version