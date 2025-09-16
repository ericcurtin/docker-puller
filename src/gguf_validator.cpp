#include "../include/gguf_validator.h"
#include <cstdio>
#include <cstdint>
#include <iostream>
#include <vector>

bool GGUFValidator::validate_file(const std::string& file_path) {
    FILE* file = fopen(file_path.c_str(), "rb");
    if (!file) {
        std::cerr << "Error: Cannot open file " << file_path << std::endl;
        return false;
    }
    
    GGUFHeader header;
    if (fread(&header, sizeof(header), 1, file) != 1) {
        std::cerr << "Error: Cannot read GGUF header" << std::endl;
        fclose(file);
        return false;
    }
    
    if (!validate_header(header)) {
        fclose(file);
        return false;
    }
    
    // Validate metadata if present
    if (header.metadata_kv_count > 0) {
        if (!validate_metadata(file, header.metadata_kv_count)) {
            fclose(file);
            return false;
        }
    }
    
    fclose(file);
    std::cout << "GGUF validation passed: " << file_path << std::endl;
    std::cout << "  Version: " << header.version << std::endl;
    std::cout << "  Tensor count: " << header.tensor_count << std::endl;
    std::cout << "  Metadata KV count: " << header.metadata_kv_count << std::endl;
    
    return true;
}

bool GGUFValidator::validate_header(const GGUFHeader& header) {
    if (header.magic != GGUF_MAGIC) {
        std::cerr << "Error: Invalid GGUF magic number. Expected: 0x" 
                  << std::hex << GGUF_MAGIC << ", got: 0x" << header.magic << std::dec << std::endl;
        return false;
    }
    
    if (header.version != GGUF_VERSION) {
        std::cerr << "Error: Unsupported GGUF version. Expected: " 
                  << GGUF_VERSION << ", got: " << header.version << std::endl;
        return false;
    }
    
    // Basic sanity checks
    if (header.tensor_count > 1000000) {  // Reasonable upper limit
        std::cerr << "Error: Tensor count seems too large: " << header.tensor_count << std::endl;
        return false;
    }
    
    if (header.metadata_kv_count > 1000) {  // Reasonable upper limit
        std::cerr << "Error: Metadata KV count seems too large: " << header.metadata_kv_count << std::endl;
        return false;
    }
    
    return true;
}

bool GGUFValidator::validate_metadata(FILE* file, uint64_t kv_count) {
    // Basic validation - read through metadata entries
    for (uint64_t i = 0; i < kv_count; ++i) {
        // Read key length
        uint64_t key_len;
        if (fread(&key_len, sizeof(key_len), 1, file) != 1) {
            std::cerr << "Error: Cannot read metadata key length" << std::endl;
            return false;
        }
        
        if (key_len > 1000) {  // Reasonable key length limit
            std::cerr << "Error: Metadata key length too large: " << key_len << std::endl;
            return false;
        }
        
        // Skip key data
        if (fseek(file, key_len, SEEK_CUR) != 0) {
            std::cerr << "Error: Cannot seek past metadata key" << std::endl;
            return false;
        }
        
        // Read value type
        uint32_t value_type;
        if (fread(&value_type, sizeof(value_type), 1, file) != 1) {
            std::cerr << "Error: Cannot read metadata value type" << std::endl;
            return false;
        }
        
        // Skip value based on type (simplified validation)
        size_t value_size = 0;
        switch (value_type) {
            case 0: // UINT8
            case 1: // INT8
                value_size = 1;
                break;
            case 2: // UINT16
            case 3: // INT16
                value_size = 2;
                break;
            case 4: // UINT32
            case 5: // INT32
            case 6: // FLOAT32
                value_size = 4;
                break;
            case 7: // BOOL
                value_size = 1;
                break;
            case 8: // STRING
                {
                    uint64_t str_len;
                    if (fread(&str_len, sizeof(str_len), 1, file) != 1) {
                        std::cerr << "Error: Cannot read string length" << std::endl;
                        return false;
                    }
                    value_size = str_len;
                }
                break;
            case 9: // ARRAY
                // Array handling is more complex, skip for basic validation
                return true;
            case 10: // UINT64
            case 11: // INT64
            case 12: // FLOAT64
                value_size = 8;
                break;
            default:
                std::cerr << "Error: Unknown metadata value type: " << value_type << std::endl;
                return false;
        }
        
        if (fseek(file, value_size, SEEK_CUR) != 0) {
            std::cerr << "Error: Cannot seek past metadata value" << std::endl;
            return false;
        }
    }
    
    return true;
}