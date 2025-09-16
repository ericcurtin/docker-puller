# Docker AI Model Puller

A high-performance C++ tool for downloading Docker AI models in GGUF format using libcurl's multi-interface with support for concurrent connections, resumable downloads, and automatic retries.

## Features

- **Multi-connection downloads**: Accelerate downloads using multiple concurrent connections (1-16 connections)
- **Resumable downloads**: Automatically resume interrupted downloads using HTTP Range headers
- **Automatic retries**: 3 automatic retries on failure by default (configurable)
- **GGUF validation**: Validates downloaded files to ensure they are valid GGUF v3 format
- **Docker registry integration**: Directly download from Docker Hub and other registries
- **Progress tracking**: Real-time download progress with speed and connection count

## Supported Models

The tool supports Docker AI models with the media type `application/vnd.docker.ai.gguf.v3`. Examples:

- `ai/smollm2:135M-Q4_0` - SmolLM2 135M parameter model, Q4_0 quantization
- `ai/smollm2:1.7B-Q4_0` - SmolLM2 1.7B parameter model, Q4_0 quantization
- Other models following the same format

## Building

### Prerequisites

- C++17 compatible compiler (GCC 7+ or Clang 6+)
- CMake 3.12+
- libcurl development headers
- jsoncpp development library

### Ubuntu/Debian

```bash
sudo apt update
sudo apt install -y build-essential cmake libcurl4-openssl-dev libjsoncpp-dev
```

### Build Instructions

```bash
git clone https://github.com/ericcurtin/docker-puller.git
cd docker-puller
mkdir build && cd build
cmake ..
make
```

### Running Tests

```bash
# Run basic functionality tests
make test

# Or run individual test executables
./test_basic
```

## Usage

### Basic Usage

```bash
# Download a model with default settings (1 connection)
./docker-puller ai/smollm2:135M-Q4_0

# Download with 4 concurrent connections
./docker-puller -c 4 ai/smollm2:135M-Q4_0

# Download with verbose output
./docker-puller -v -c 4 ai/smollm2:135M-Q4_0

# Dry run (validate without downloading)
./docker-puller -n ai/smollm2:135M-Q4_0
```

### Command Line Options

```
Usage: ./docker-puller [OPTIONS] MODEL_SPEC

Download Docker AI models in GGUF format

Arguments:
  MODEL_SPEC    Model specification (e.g., ai/smollm2:135M-Q4_0)

Options:
  -c, --connections NUM  Number of concurrent connections (default: 1)
  -o, --output PATH      Output file path (default: model filename)
  -r, --retries NUM      Number of retry attempts (default: 3)
  -v, --verbose          Verbose output
  -n, --dry-run          Dry run (don't actually download)
  -h, --help             Show this help message
```

### Examples

```bash
# Download with maximum performance (8 connections)
./docker-puller -c 8 -v ai/smollm2:135M-Q4_0

# Download to specific location with 4 connections
./docker-puller -c 4 -o /models/smollm2.gguf ai/smollm2:135M-Q4_0

# Download with 5 retry attempts
./docker-puller -r 5 ai/smollm2:135M-Q4_0

# Test model availability without downloading
./docker-puller -n ai/smollm2:135M-Q4_0
```

## Performance

### Optimal Connection Count

Based on testing with the `ai/smollm2:135M-Q4_0` model (~87.5 MB), the optimal number of concurrent connections depends on your network conditions:

| Connections | Use Case | Notes |
|-------------|----------|-------|
| 1 | Stable/slow networks | Single connection, most reliable |
| 2-4 | Recommended | Good balance of speed and reliability |
| 8+ | High-bandwidth networks | Maximum performance, may stress server |

**Recommendation**: Start with 4 connections for most use cases.

### Performance Testing

Run the included performance test script to benchmark different connection counts:

```bash
# From the build directory
../performance_test.sh
```

## Technical Details

### Architecture

- **Docker Registry Client**: Handles authentication and manifest parsing
- **Multi-Connection Downloader**: Uses libcurl multi-interface for concurrent downloads
- **GGUF Validator**: Validates file format and integrity
- **Resumable Downloads**: Uses HTTP Range headers for chunk-based downloading

### GGUF Format Support

The tool specifically validates GGUF v3 format files with:
- Magic number: `GGUF` (0x46554747)
- Version: 3
- Proper header structure validation

### Error Handling

- Automatic retry on network failures
- Resumable downloads on interruption
- GGUF format validation after download
- Comprehensive error reporting

## Docker AI Model Format

This tool works with Docker AI models that use the following media type:
```
application/vnd.docker.ai.gguf.v3
```

Models are referenced using the format: `[registry/]namespace/repository:tag`

Examples:
- `ai/smollm2:135M-Q4_0`
- `registry.example.com/myorg/mymodel:v1.0`

## Contributing

1. Fork the repository
2. Create a feature branch
3. Implement your changes with tests
4. Run the test suite: `make test`
5. Submit a pull request

## License

This project is licensed under the MIT License - see the LICENSE file for details.

## Verification

To verify the downloaded GGUF files are valid:

1. The tool automatically validates GGUF format after download
2. You can manually verify using the GGUF specification
3. Use with llama.cpp or other GGUF-compatible tools

## Troubleshooting

### Common Issues

1. **Authentication errors**: Ensure Docker registry is accessible
2. **Network timeouts**: Try reducing connection count
3. **Disk space**: Ensure sufficient space for the model file
4. **Permission errors**: Check write permissions to output directory

### Debug Mode

Use verbose mode (`-v`) to see detailed debug information:

```bash
./docker-puller -v -c 4 ai/smollm2:135M-Q4_0
```

### Validation Test

Test with the known working model:

```bash
./docker-puller -v -n ai/smollm2:135M-Q4_0
```

This should show:
- Successful parsing of model specification
- Valid authentication token
- Found GGUF layer (87.5 MB)
- Valid download URL