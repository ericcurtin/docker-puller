#include "gguf_validator.h"
#include <fstream>
#include <iostream>

bool GGUFValidator::validate_file(const std::string& file_path) {
    GGUFInfo info = get_file_info(file_path);
    return info.valid;
}

GGUFValidator::GGUFInfo GGUFValidator::get_file_info(const std::string& file_path) {
    GGUFInfo info = {};
    info.valid = read_header(file_path, info);
    return info;
}

bool GGUFValidator::read_header(const std::string& file_path, GGUFInfo& info) {
    std::ifstream file(file_path, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Failed to open file: " << file_path << std::endl;
        return false;
    }
    
    // Read magic number (4 bytes)
    uint32_t magic;
    if (!file.read(reinterpret_cast<char*>(&magic), sizeof(magic))) {
        std::cerr << "Failed to read magic number" << std::endl;
        return false;
    }
    
    if (magic != GGUF_MAGIC) {
        std::cerr << "Invalid GGUF magic number. Expected: 0x" << std::hex << GGUF_MAGIC 
                  << ", got: 0x" << magic << std::dec << std::endl;
        return false;
    }
    
    // Read version (4 bytes)
    if (!file.read(reinterpret_cast<char*>(&info.version), sizeof(info.version))) {
        std::cerr << "Failed to read version" << std::endl;
        return false;
    }
    
    if (info.version != SUPPORTED_VERSION) {
        std::cerr << "Unsupported GGUF version: " << info.version 
                  << ". Supported version: " << SUPPORTED_VERSION << std::endl;
        return false;
    }
    
    // Read tensor count (8 bytes)
    if (!file.read(reinterpret_cast<char*>(&info.tensor_count), sizeof(info.tensor_count))) {
        std::cerr << "Failed to read tensor count" << std::endl;
        return false;
    }
    
    // Read metadata key-value count (8 bytes)
    if (!file.read(reinterpret_cast<char*>(&info.metadata_kv_count), sizeof(info.metadata_kv_count))) {
        std::cerr << "Failed to read metadata kv count" << std::endl;
        return false;
    }
    
    std::cout << "GGUF file validation successful:" << std::endl;
    std::cout << "  Version: " << info.version << std::endl;
    std::cout << "  Tensor count: " << info.tensor_count << std::endl;
    std::cout << "  Metadata KV count: " << info.metadata_kv_count << std::endl;
    
    return true;
}