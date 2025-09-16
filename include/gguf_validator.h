#pragma once

#include <string>
#include <cstdint>

class GgufValidator {
public:
    static bool validate_file(const std::string& file_path);
    static bool is_valid_magic(const std::string& file_path);
    
private:
    static constexpr uint32_t GGUF_MAGIC = 0x46554747; // "GGUF" in little-endian
    static constexpr uint32_t GGUF_VERSION = 3;
};