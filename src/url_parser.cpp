#include "url_parser.h"
#include <regex>
#include <iostream>

const std::string UrlParser::REGISTRY_BASE_URL = "https://registry-1.docker.io/v2";
const std::string UrlParser::MEDIA_TYPE = "application/vnd.docker.ai.gguf.v3";

ModelSpec UrlParser::parse_model_spec(const std::string& model_spec) {
    ModelSpec spec;
    
    // Parse format like "ai/smollm2:135M-Q4_0"
    std::regex pattern(R"(^([^/]+)/([^:]+):(.+)$)");
    std::smatch matches;
    
    if (std::regex_match(model_spec, matches, pattern)) {
        spec.namespace_name = matches[1].str();
        spec.model_name = matches[2].str();
        spec.tag = matches[3].str();
    }
    
    return spec;
}

std::string UrlParser::build_download_url(const ModelSpec& spec) {
    if (!spec.is_valid()) {
        return "";
    }
    
    // Build Docker registry API URL for blob download
    // This is a simplified implementation - in reality, we'd need to:
    // 1. Get the manifest from the registry
    // 2. Extract the blob digest
    // 3. Download the blob
    // For now, we'll simulate a direct download URL
    return REGISTRY_BASE_URL + "/" + spec.namespace_name + "/" + spec.model_name + 
           "/blobs/" + spec.tag + ".gguf";
}

bool UrlParser::is_valid_model_spec(const std::string& model_spec) {
    ModelSpec spec = parse_model_spec(model_spec);
    return spec.is_valid();
}