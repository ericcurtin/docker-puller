#include "gguf_validator.h"
#include <fstream>
#include <cstdint>

bool GgufValidator::validate_file(const std::string& file_path) {
    std::ifstream file(file_path, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }
    
    // Read magic number (4 bytes)
    uint32_t magic;
    file.read(reinterpret_cast<char*>(&magic), sizeof(magic));
    if (file.gcount() != sizeof(magic)) {
        return false;
    }
    
    // Check magic number
    if (magic != GGUF_MAGIC) {
        return false;
    }
    
    // Read version (4 bytes)
    uint32_t version;
    file.read(reinterpret_cast<char*>(&version), sizeof(version));
    if (file.gcount() != sizeof(version)) {
        return false;
    }
    
    // Check version (we support version 3)
    if (version != GGUF_VERSION) {
        return false;
    }
    
    // Read tensor count (8 bytes)
    uint64_t tensor_count;
    file.read(reinterpret_cast<char*>(&tensor_count), sizeof(tensor_count));
    if (file.gcount() != sizeof(tensor_count)) {
        return false;
    }
    
    // Read metadata key-value count (8 bytes)
    uint64_t metadata_kv_count;
    file.read(reinterpret_cast<char*>(&metadata_kv_count), sizeof(metadata_kv_count));
    if (file.gcount() != sizeof(metadata_kv_count)) {
        return false;
    }
    
    // Basic validation: reasonable limits
    if (tensor_count > 10000 || metadata_kv_count > 10000) {
        return false;
    }
    
    return true;
}

bool GgufValidator::is_valid_magic(const std::string& file_path) {
    std::ifstream file(file_path, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }
    
    uint32_t magic;
    file.read(reinterpret_cast<char*>(&magic), sizeof(magic));
    if (file.gcount() != sizeof(magic)) {
        return false;
    }
    
    return magic == GGUF_MAGIC;
}