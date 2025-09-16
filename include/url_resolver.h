#pragma once

#include <string>

class UrlResolver {
public:
    static std::string resolve_docker_ai_model_url(const std::string& model_spec);
    static bool is_valid_docker_ai_model(const std::string& model_spec);
    
private:
    static std::string get_registry_url();
    static std::string parse_model_name(const std::string& model_spec);
    static std::string parse_model_tag(const std::string& model_spec);
    static std::string query_registry_manifest(const std::string& model_name, const std::string& tag);
};