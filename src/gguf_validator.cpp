#include "gguf_validator.hpp"
#include <fstream>
#include <iostream>
#include <cstring>

bool GGUFValidator::validate(const std::string& file_path) {
    GGUFHeader header;
    size_t file_size;
    
    if (!getFileInfo(file_path, header, file_size)) {
        return false;
    }
    
    // Check magic number
    if (header.magic != GGUF_MAGIC) {
        std::cerr << "Invalid GGUF magic number: 0x" << std::hex << header.magic << std::endl;
        return false;
    }
    
    // Check version
    if (header.version < MIN_SUPPORTED_VERSION || header.version > MAX_SUPPORTED_VERSION) {
        std::cerr << "Unsupported GGUF version: " << header.version << std::endl;
        return false;
    }
    
    // Basic sanity checks
    if (header.tensor_count > 100000) {  // Reasonable upper limit
        std::cerr << "Suspicious tensor count: " << header.tensor_count << std::endl;
        return false;
    }
    
    if (header.metadata_kv_count > 10000) {  // Reasonable upper limit
        std::cerr << "Suspicious metadata count: " << header.metadata_kv_count << std::endl;
        return false;
    }
    
    // File size check - minimum size for a valid GGUF file
    if (file_size < sizeof(GGUFHeader)) {
        std::cerr << "File too small to be valid GGUF: " << file_size << " bytes" << std::endl;
        return false;
    }
    
    std::cout << "GGUF validation successful:" << std::endl;
    std::cout << "  Version: " << header.version << std::endl;
    std::cout << "  Tensor count: " << header.tensor_count << std::endl;
    std::cout << "  Metadata entries: " << header.metadata_kv_count << std::endl;
    std::cout << "  File size: " << file_size << " bytes" << std::endl;
    
    return true;
}

bool GGUFValidator::readHeader(const std::string& file_path, GGUFHeader& header) {
    std::ifstream file(file_path, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Cannot open file: " << file_path << std::endl;
        return false;
    }
    
    // Read the header
    file.read(reinterpret_cast<char*>(&header), sizeof(GGUFHeader));
    if (!file) {
        std::cerr << "Cannot read GGUF header from file: " << file_path << std::endl;
        return false;
    }
    
    return true;
}

bool GGUFValidator::hasValidMagic(const std::string& file_path) {
    std::ifstream file(file_path, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }
    
    uint32_t magic;
    file.read(reinterpret_cast<char*>(&magic), sizeof(magic));
    if (!file) {
        return false;
    }
    
    return magic == GGUF_MAGIC;
}

bool GGUFValidator::getFileInfo(const std::string& file_path, GGUFHeader& header, size_t& file_size) {
    if (!readHeader(file_path, header)) {
        return false;
    }
    
    // Get file size
    std::ifstream file(file_path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        std::cerr << "Cannot open file to get size: " << file_path << std::endl;
        return false;
    }
    
    file_size = file.tellg();
    return true;
}