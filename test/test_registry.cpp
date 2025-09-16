#include <iostream>
#include "../include/docker_registry.h"

int main() {
    try {
        DockerAIModel model = DockerAIModel::parse("ai/smollm2:135M-Q4_0");
        std::cout << "Testing Docker registry connection for: " << model.to_string() << "\n";
        
        DockerRegistry registry;
        std::cout << "Getting auth token...\n";
        std::string token = registry.get_auth_token(model);
        std::cout << "Token length: " << token.length() << "\n";
        
        std::cout << "Getting manifest layers...\n";
        auto layers = registry.get_manifest_layers(model);
        std::cout << "Found " << layers.size() << " layers\n";
        
        for (const auto& layer : layers) {
            std::cout << "Layer: " << layer.media_type << " (" << layer.size << " bytes)\n";
            std::cout << "  Digest: " << layer.digest << "\n";
        }
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}