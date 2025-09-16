#pragma once

#include <string>
#include <cstdint>

/**
 * GGUF file format validator
 * Based on GGUF specification for validating downloaded AI models
 */
class GGUFValidator {
public:
    /**
     * Validate that the file is a valid GGUF file
     */
    static bool validate_file(const std::string& file_path);
    
    /**
     * Get GGUF file information
     */
    struct GGUFInfo {
        uint32_t version;
        uint64_t tensor_count;
        uint64_t metadata_kv_count;
        bool valid;
    };
    
    static GGUFInfo get_file_info(const std::string& file_path);
    
private:
    static const uint32_t GGUF_MAGIC = 0x46554747; // "GGUF" in little endian
    static const uint32_t SUPPORTED_VERSION = 3;
    
    static bool read_header(const std::string& file_path, GGUFInfo& info);
};