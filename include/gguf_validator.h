#pragma once
#include <string>
#include <cstdint>
#include <cstdio>

class GGUFValidator {
public:
    static bool validate_file(const std::string& file_path);
    
private:
    static constexpr uint32_t GGUF_MAGIC = 0x46554747; // "GGUF"
    static constexpr uint32_t GGUF_VERSION = 3;
    
    struct GGUFHeader {
        uint32_t magic;
        uint32_t version;
        uint64_t tensor_count;
        uint64_t metadata_kv_count;
    };
    
    static bool validate_header(const GGUFHeader& header);
    static bool validate_metadata(FILE* file, uint64_t kv_count);
};