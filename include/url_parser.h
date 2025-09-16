#pragma once

#include <string>
#include <vector>

/**
 * Represents a Docker AI model reference
 */
struct DockerAIModel {
    std::string registry;
    std::string namespace_name;
    std::string repository;
    std::string tag;
    
    /**
     * Full model identifier (e.g., "ai/smollm2:135M-Q4_0")
     */
    std::string full_name() const;
    
    /**
     * Generate manifest URL for the model
     */
    std::string manifest_url() const;
    
    /**
     * Generate blob URL for a specific digest
     */
    std::string blob_url(const std::string& digest) const;
};

/**
 * Parse Docker AI model URL from string format
 * Supports formats like: ai/smollm2:135M-Q4_0
 */
class URLParser {
public:
    static DockerAIModel parse(const std::string& model_ref);
    
private:
    static std::string default_registry();
    static std::vector<std::string> split(const std::string& str, char delimiter);
};