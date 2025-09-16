#pragma once

#include "url_parser.h"
#include "multi_downloader.h"
#include "gguf_validator.h"
#include <string>
#include <memory>

/**
 * Main Docker AI model puller class
 */
class DockerAIPuller {
public:
    explicit DockerAIPuller(int max_connections = 1);
    ~DockerAIPuller();
    
    /**
     * Pull a Docker AI model
     */
    bool pull_model(const std::string& model_ref, const std::string& output_dir = ".");
    
    /**
     * Set authentication token for registry access
     */
    void set_auth_token(const std::string& token);
    
    /**
     * Set progress callback for download progress
     */
    void set_progress_callback(ProgressCallback callback);
    
private:
    int max_connections_;
    std::string auth_token_;
    std::unique_ptr<MultiDownloader> downloader_;
    
    // Docker registry API methods
    std::string get_manifest(const DockerAIModel& model);
    std::string get_blob_digest_for_gguf(const std::string& manifest_json);
    std::string get_auth_token_for_registry(const DockerAIModel& model);
    
    // Helper methods
    std::string generate_output_filename(const DockerAIModel& model);
    bool validate_downloaded_file(const std::string& file_path);
    
    // Static curl callback
    static size_t write_string_callback(void* contents, size_t size, size_t nmemb, void* userp);
};