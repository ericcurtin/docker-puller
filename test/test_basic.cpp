#include <iostream>
#include <cassert>
#include <fstream>
#include "../include/docker_registry.h"
#include "../include/gguf_validator.h"

void test_model_parsing() {
    std::cout << "Testing model parsing...\n";
    
    // Test basic AI model parsing
    DockerAIModel model = DockerAIModel::parse("ai/smollm2:135M-Q4_0");
    assert(model.namespace_name == "ai");
    assert(model.repository == "smollm2");
    assert(model.tag == "135M-Q4_0");
    assert(model.registry == "registry-1.docker.io");
    
    std::cout << "Parsed model: " << model.to_string() << "\n";
    std::cout << "✓ Model parsing test passed\n";
}

void test_gguf_validation() {
    std::cout << "Testing GGUF validation...\n";
    
    // Create a test file with GGUF magic
    std::ofstream test_file("test.gguf", std::ios::binary);
    uint32_t magic = 0x46554747; // "GGUF"
    uint32_t version = 3;
    uint64_t tensor_count = 0;
    uint64_t metadata_count = 0;
    
    test_file.write(reinterpret_cast<const char*>(&magic), sizeof(magic));
    test_file.write(reinterpret_cast<const char*>(&version), sizeof(version));
    test_file.write(reinterpret_cast<const char*>(&tensor_count), sizeof(tensor_count));
    test_file.write(reinterpret_cast<const char*>(&metadata_count), sizeof(metadata_count));
    test_file.close();
    
    assert(GgufValidator::validate_file("test.gguf"));
    assert(GgufValidator::is_valid_magic("test.gguf"));
    
    // Cleanup
    std::remove("test.gguf");
    
    std::cout << "✓ GGUF validation test passed\n";
}

int main() {
    try {
        test_model_parsing();
        test_gguf_validation();
        std::cout << "All tests passed!\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed: " << e.what() << "\n";
        return 1;
    }
}