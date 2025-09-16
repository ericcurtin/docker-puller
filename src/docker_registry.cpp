#include "docker_registry.h"
#include "utils.h"

#include <curl/curl.h>
#include <json/json.h>
#include <stdexcept>
#include <sstream>
#include <regex>
#include <iostream>

DockerAIModel DockerAIModel::parse(const std::string& model_spec) {
    DockerAIModel model;
    
    std::regex pattern(R"(^(?:([^/]+)/)?([^:]+):(.+)$)");
    std::smatch matches;
    
    if (!std::regex_match(model_spec, matches, pattern)) {
        throw std::invalid_argument("Invalid model specification: " + model_spec);
    }
    
    if (matches[1].matched && !matches[1].str().empty()) {
        model.namespace_name = matches[1].str();
    } else {
        model.namespace_name = "ai";  // Default namespace for AI models
    }
    
    model.repository = matches[2].str();
    model.tag = matches[3].str();
    
    return model;
}

std::string DockerAIModel::to_string() const {
    return namespace_name + "/" + repository + ":" + tag;
}

struct WriteCallbackData {
    std::string* buffer;
};

static size_t WriteCallback(void* contents, size_t size, size_t nmemb, WriteCallbackData* data) {
    size_t total_size = size * nmemb;
    data->buffer->append((char*)contents, total_size);
    return total_size;
}

class DockerRegistry::Impl {
public:
    Impl() {
        curl_global_init(CURL_GLOBAL_DEFAULT);
        curl_handle_ = curl_easy_init();
        if (!curl_handle_) {
            throw std::runtime_error("Failed to initialize CURL");
        }
        
        // Set common options
        curl_easy_setopt(curl_handle_, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl_handle_, CURLOPT_USERAGENT, "docker-puller/1.0");
        curl_easy_setopt(curl_handle_, CURLOPT_TIMEOUT, 30L);
        curl_easy_setopt(curl_handle_, CURLOPT_WRITEFUNCTION, WriteCallback);
    }
    
    ~Impl() {
        if (curl_handle_) {
            curl_easy_cleanup(curl_handle_);
        }
        curl_global_cleanup();
    }
    
    std::string make_request(const std::string& url, const std::vector<std::string>& headers = {}) {
        std::string response;
        WriteCallbackData callback_data{&response};
        
        curl_easy_setopt(curl_handle_, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl_handle_, CURLOPT_WRITEDATA, &callback_data);
        
        struct curl_slist* header_list = nullptr;
        for (const auto& header : headers) {
            header_list = curl_slist_append(header_list, header.c_str());
        }
        
        if (header_list) {
            curl_easy_setopt(curl_handle_, CURLOPT_HTTPHEADER, header_list);
        }
        
        CURLcode res = curl_easy_perform(curl_handle_);
        
        if (header_list) {
            curl_slist_free_all(header_list);
        }
        
        if (res != CURLE_OK) {
            throw std::runtime_error("CURL request failed: " + std::string(curl_easy_strerror(res)));
        }
        
        long response_code;
        curl_easy_getinfo(curl_handle_, CURLINFO_RESPONSE_CODE, &response_code);
        
        if (response_code >= 400) {
            throw std::runtime_error("HTTP error " + std::to_string(response_code) + " for URL: " + url);
        }
        
        return response;
    }
    
    std::string get_auth_token(const DockerAIModel& model) {
        std::string auth_url = "https://auth.docker.io/token?service=registry.docker.io&scope=repository:" + 
                              model.namespace_name + "/" + model.repository + ":pull";
        
        std::string response = make_request(auth_url);
        
        Json::Value root;
        Json::Reader reader;
        
        if (!reader.parse(response, root)) {
            throw std::runtime_error("Failed to parse auth response JSON");
        }
        
        if (!root.isMember("token")) {
            throw std::runtime_error("No token found in auth response");
        }
        
        return root["token"].asString();
    }
    
    std::vector<ManifestLayer> get_manifest_layers(const DockerAIModel& model) {
        std::string token = get_auth_token(model);
        
        std::string manifest_url = "https://" + model.registry + "/v2/" + 
                                  model.namespace_name + "/" + model.repository + "/manifests/" + model.tag;
        
        std::vector<std::string> headers = {
            "Authorization: Bearer " + token,
            "Accept: application/vnd.docker.distribution.manifest.v2+json,application/vnd.oci.image.manifest.v1+json"
        };
        
        std::string response = make_request(manifest_url, headers);
        
        Json::Value root;
        Json::Reader reader;
        
        if (!reader.parse(response, root)) {
            throw std::runtime_error("Failed to parse manifest JSON");
        }
        
        std::vector<ManifestLayer> layers;
        
        if (root.isMember("layers")) {
            const Json::Value& json_layers = root["layers"];
            for (const auto& layer : json_layers) {
                if (layer.isMember("mediaType") && layer.isMember("digest") && layer.isMember("size")) {
                    ManifestLayer manifest_layer;
                    manifest_layer.media_type = layer["mediaType"].asString();
                    manifest_layer.digest = layer["digest"].asString();
                    manifest_layer.size = layer["size"].asUInt64();
                    layers.push_back(manifest_layer);
                }
            }
        }
        
        // Also check for GGUF media type in the config or other locations
        if (root.isMember("config")) {
            const Json::Value& config = root["config"];
            if (config.isMember("mediaType") && config.isMember("digest") && config.isMember("size")) {
                std::string media_type = config["mediaType"].asString();
                if (media_type == "application/vnd.docker.ai.gguf.v3") {
                    ManifestLayer manifest_layer;
                    manifest_layer.media_type = media_type;
                    manifest_layer.digest = config["digest"].asString();
                    manifest_layer.size = config["size"].asUInt64();
                    layers.push_back(manifest_layer);
                }
            }
        }
        
        return layers;
    }
    
    std::string get_download_url(const DockerAIModel& model, const std::string& digest) {
        return "https://" + model.registry + "/v2/" + 
               model.namespace_name + "/" + model.repository + "/blobs/" + digest;
    }

private:
    CURL* curl_handle_;
};

DockerRegistry::DockerRegistry() : impl_(std::make_unique<Impl>()) {}

DockerRegistry::~DockerRegistry() = default;

std::vector<ManifestLayer> DockerRegistry::get_manifest_layers(const DockerAIModel& model) {
    return impl_->get_manifest_layers(model);
}

std::string DockerRegistry::get_download_url(const DockerAIModel& model, const std::string& digest) {
    return impl_->get_download_url(model, digest);
}

std::string DockerRegistry::get_auth_token(const DockerAIModel& model) {
    return impl_->get_auth_token(model);
}