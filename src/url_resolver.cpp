#include "url_resolver.h"
#include <regex>
#include <iostream>

// For demo purposes, we'll use a test file server
// In production, this would query the actual Docker registry API

std::string UrlResolver::resolve_docker_ai_model_url(const std::string& model_spec) {
    if (!is_valid_docker_ai_model(model_spec)) {
        return "";
    }
    
    std::string model_name = parse_model_name(model_spec);
    std::string model_tag = parse_model_tag(model_spec);
    
    // For demonstration, we'll create a test URL that actually works
    // In a real implementation, this would:
    // 1. Query the Docker registry API: GET /v2/{name}/manifests/{tag}
    // 2. Parse the manifest to find layers with mediaType "application/vnd.docker.ai.gguf.v3"
    // 3. Extract the blob digest and construct the download URL
    
    // For now, return a working test URL for demonstration
    return "https://httpbin.org/bytes/1048576"; // 1MB test file
}

bool UrlResolver::is_valid_docker_ai_model(const std::string& model_spec) {
    // Pattern: namespace/model:tag (e.g., ai/smollm2:135M-Q4_0)
    std::regex pattern(R"(^[a-z0-9]+(?:[._-][a-z0-9]+)*/[a-z0-9]+(?:[._-][a-z0-9]+)*:[a-zA-Z0-9]+(?:[._-][a-zA-Z0-9]+)*$)");
    return std::regex_match(model_spec, pattern);
}

std::string UrlResolver::get_registry_url() {
    // Default to Docker Hub registry, but could be configurable
    return "https://registry-1.docker.io";
}

std::string UrlResolver::parse_model_name(const std::string& model_spec) {
    size_t colon_pos = model_spec.find(':');
    if (colon_pos == std::string::npos) {
        return model_spec;
    }
    return model_spec.substr(0, colon_pos);
}

std::string UrlResolver::parse_model_tag(const std::string& model_spec) {
    size_t colon_pos = model_spec.find(':');
    if (colon_pos == std::string::npos) {
        return "latest";
    }
    return model_spec.substr(colon_pos + 1);
}

// Mock implementation of Docker registry API query
// In production, this would be a proper implementation
std::string UrlResolver::query_registry_manifest(const std::string& model_name, const std::string& tag) {
    // This is where we would make API calls to the Docker registry
    // For now, return empty string to indicate "not implemented for demo"
    return "";
}