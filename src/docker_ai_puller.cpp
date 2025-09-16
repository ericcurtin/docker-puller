#include "docker_ai_puller.h"
#include <nlohmann/json.hpp>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <regex>

using json = nlohmann::json;

DockerAIPuller::DockerAIPuller(int max_connections) 
    : max_connections_(max_connections) {
    downloader_ = std::make_unique<MultiDownloader>(max_connections_);
}

DockerAIPuller::~DockerAIPuller() = default;

void DockerAIPuller::set_auth_token(const std::string& token) {
    auth_token_ = token;
}

void DockerAIPuller::set_progress_callback(ProgressCallback callback) {
    if (downloader_) {
        downloader_->set_progress_callback(callback);
    }
}

bool DockerAIPuller::pull_model(const std::string& model_ref, const std::string& output_dir) {
    try {
        // Parse the model reference
        DockerAIModel model = URLParser::parse(model_ref);
        std::cout << "Pulling model: " << model.full_name() << std::endl;
        std::cout << "Registry: " << model.registry << std::endl;
        
        // Get authentication token if needed
        if (auth_token_.empty()) {
            auth_token_ = get_auth_token_for_registry(model);
        }
        
        // Get manifest
        std::string manifest_json = get_manifest(model);
        if (manifest_json.empty()) {
            std::cerr << "Failed to get manifest for model: " << model.full_name() << std::endl;
            return false;
        }
        
        // Extract GGUF blob digest from manifest
        std::string gguf_digest = get_blob_digest_for_gguf(manifest_json);
        if (gguf_digest.empty()) {
            std::cerr << "No GGUF blob found in manifest" << std::endl;
            return false;
        }
        
        std::cout << "GGUF blob digest: " << gguf_digest << std::endl;
        
        // Generate output filename
        std::string output_filename = generate_output_filename(model);
        std::string output_path = output_dir + "/" + output_filename;
        
        // Ensure output directory exists
        std::filesystem::create_directories(output_dir);
        
        // Download the GGUF blob
        std::string blob_url = model.blob_url(gguf_digest);
        std::cout << "Downloading from: " << blob_url << std::endl;
        std::cout << "Output path: " << output_path << std::endl;
        
        bool download_success = downloader_->download_file(blob_url, output_path, auth_token_);
        if (!download_success) {
            std::cerr << "Failed to download GGUF blob" << std::endl;
            return false;
        }
        
        // Validate the downloaded file
        if (!validate_downloaded_file(output_path)) {
            std::cerr << "Downloaded file validation failed" << std::endl;
            return false;
        }
        
        std::cout << "Model downloaded and validated successfully: " << output_path << std::endl;
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "Error pulling model: " << e.what() << std::endl;
        return false;
    }
}

std::string DockerAIPuller::get_manifest(const DockerAIModel& model) {
    std::string manifest_url = model.manifest_url();
    
    // For Docker AI models, we need to set the correct Accept header
    CURL* curl = curl_easy_init();
    if (!curl) return "";
    
    std::string response;
    curl_easy_setopt(curl, CURLOPT_URL, manifest_url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, [](void* contents, size_t size, size_t nmemb, void* userp) -> size_t {
        size_t total_size = size * nmemb;
        static_cast<std::string*>(userp)->append(static_cast<const char*>(contents), total_size);
        return total_size;
    });
    
    // Set headers for Docker AI manifest
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Accept: application/vnd.docker.ai.gguf.v3+json");
    headers = curl_slist_append(headers, "Accept: application/vnd.docker.distribution.manifest.v2+json");
    
    if (!auth_token_.empty()) {
        std::string auth_header = "Authorization: Bearer " + auth_token_;
        headers = curl_slist_append(headers, auth_header.c_str());
    }
    
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "docker-ai-puller/1.0");
    
    CURLcode res = curl_easy_perform(curl);
    
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    
    if (res != CURLE_OK) {
        std::cerr << "Failed to get manifest: " << curl_easy_strerror(res) << std::endl;
        return "";
    }
    
    return response;
}

std::string DockerAIPuller::get_blob_digest_for_gguf(const std::string& manifest_json) {
    try {
        json manifest = json::parse(manifest_json);
        
        // Look for GGUF layer in the manifest
        if (manifest.contains("layers")) {
            for (const auto& layer : manifest["layers"]) {
                if (layer.contains("mediaType")) {
                    std::string media_type = layer["mediaType"];
                    if (media_type == "application/vnd.docker.ai.gguf.v3" || 
                        media_type.find("gguf") != std::string::npos) {
                        if (layer.contains("digest")) {
                            return layer["digest"];
                        }
                    }
                }
            }
        }
        
        // Fallback: try to find any blob that might be a GGUF file
        if (manifest.contains("config") && manifest["config"].contains("digest")) {
            return manifest["config"]["digest"];
        }
        
        // Another fallback: use the first layer if available
        if (manifest.contains("layers") && !manifest["layers"].empty()) {
            if (manifest["layers"][0].contains("digest")) {
                return manifest["layers"][0]["digest"];
            }
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Error parsing manifest JSON: " << e.what() << std::endl;
    }
    
    return "";
}

std::string DockerAIPuller::get_auth_token_for_registry(const DockerAIModel& model) {
    // For now, return empty string. In a real implementation, this would
    // handle Docker registry authentication flow
    std::cout << "Note: Anonymous access to registry (no authentication)" << std::endl;
    return "";
}

std::string DockerAIPuller::generate_output_filename(const DockerAIModel& model) {
    // Generate a safe filename from the model reference
    std::string filename = model.namespace_name + "_" + model.repository + "_" + model.tag + ".gguf";
    
    // Replace any unsafe characters with underscores
    std::regex unsafe_chars(R"([^a-zA-Z0-9._-])");
    filename = std::regex_replace(filename, unsafe_chars, "_");
    
    return filename;
}

bool DockerAIPuller::validate_downloaded_file(const std::string& file_path) {
    std::cout << "Validating GGUF file..." << std::endl;
    
    if (!std::filesystem::exists(file_path)) {
        std::cerr << "Downloaded file does not exist: " << file_path << std::endl;
        return false;
    }
    
    auto file_size = std::filesystem::file_size(file_path);
    std::cout << "Downloaded file size: " << file_size << " bytes" << std::endl;
    
    if (file_size == 0) {
        std::cerr << "Downloaded file is empty" << std::endl;
        return false;
    }
    
    // Validate GGUF format
    bool is_valid = GGUFValidator::validate_file(file_path);
    if (!is_valid) {
        std::cerr << "File is not a valid GGUF file" << std::endl;
        return false;
    }
    
    std::cout << "GGUF validation successful" << std::endl;
    return true;
}