#pragma once

#include <string>
#include <vector>
#include <memory>

struct DockerAIModel {
    std::string registry = "registry-1.docker.io";
    std::string namespace_name;
    std::string repository;
    std::string tag;
    
    static DockerAIModel parse(const std::string& model_spec);
    std::string to_string() const;
};

struct ManifestLayer {
    std::string media_type;
    std::string digest;
    size_t size;
    std::string download_url;
};

class DockerRegistry {
public:
    DockerRegistry();
    ~DockerRegistry();
    
    std::vector<ManifestLayer> get_manifest_layers(const DockerAIModel& model);
    std::string get_download_url(const DockerAIModel& model, const std::string& digest);
    std::string get_auth_token(const DockerAIModel& model);
    
private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};