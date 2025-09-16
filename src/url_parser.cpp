#include "url_parser.h"
#include <sstream>
#include <stdexcept>

std::string DockerAIModel::full_name() const {
    if (namespace_name.empty()) {
        return repository + ":" + tag;
    }
    return namespace_name + "/" + repository + ":" + tag;
}

std::string DockerAIModel::manifest_url() const {
    return registry + "/v2/" + namespace_name + "/" + repository + "/manifests/" + tag;
}

std::string DockerAIModel::blob_url(const std::string& digest) const {
    return registry + "/v2/" + namespace_name + "/" + repository + "/blobs/" + digest;
}

DockerAIModel URLParser::parse(const std::string& model_ref) {
    DockerAIModel model;
    model.registry = default_registry();
    
    // Split by ':'
    auto colon_pos = model_ref.find(':');
    if (colon_pos == std::string::npos) {
        throw std::invalid_argument("Invalid model reference: missing tag");
    }
    
    std::string name_part = model_ref.substr(0, colon_pos);
    model.tag = model_ref.substr(colon_pos + 1);
    
    // Split namespace and repository
    auto slash_pos = name_part.find('/');
    if (slash_pos != std::string::npos) {
        model.namespace_name = name_part.substr(0, slash_pos);
        model.repository = name_part.substr(slash_pos + 1);
    } else {
        // If no namespace, use 'library' as default for Docker Hub compatibility
        model.namespace_name = "library";
        model.repository = name_part;
    }
    
    return model;
}

std::string URLParser::default_registry() {
    return "https://registry-1.docker.io";
}

std::vector<std::string> URLParser::split(const std::string& str, char delimiter) {
    std::vector<std::string> tokens;
    std::stringstream ss(str);
    std::string token;
    
    while (std::getline(ss, token, delimiter)) {
        tokens.push_back(token);
    }
    
    return tokens;
}