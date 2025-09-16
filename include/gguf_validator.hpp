#pragma once

#include <string>
#include <cstdint>

class GGUFValidator {
public:
    struct GGUFHeader {
        uint32_t magic;
        uint32_t version;
        uint64_t tensor_count;
        uint64_t metadata_kv_count;
    };

    // Validate GGUF file format
    static bool validate(const std::string& file_path);
    
    // Read and validate GGUF header
    static bool readHeader(const std::string& file_path, GGUFHeader& header);
    
    // Check if file has correct GGUF magic number
    static bool hasValidMagic(const std::string& file_path);
    
    // Get file information
    static bool getFileInfo(const std::string& file_path, GGUFHeader& header, size_t& file_size);

private:
    static const uint32_t GGUF_MAGIC = 0x46554747; // "GGUF" in little-endian
    static const uint32_t MIN_SUPPORTED_VERSION = 1;
    static const uint32_t MAX_SUPPORTED_VERSION = 3;
};