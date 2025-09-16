#pragma once
#include <string>
#include <vector>
#include <memory>
#include <atomic>
#include <mutex>
#include <curl/curl.h>

struct DownloadStats {
    size_t total_size = 0;
    size_t downloaded = 0;
    double start_time = 0.0;
    double current_time = 0.0;
    std::atomic<bool> complete{false};
    std::atomic<int> active_connections{0};
};

struct ConnectionData {
    CURL* handle = nullptr;
    FILE* file = nullptr;
    size_t start_offset = 0;
    size_t end_offset = 0;
    size_t bytes_written = 0;
    int connection_id = 0;
    std::string url;
    std::shared_ptr<DownloadStats> stats;
    std::mutex* file_mutex = nullptr;
};

class DockerAIPuller {
public:
    DockerAIPuller(int max_connections = 1, int max_retries = 3);
    ~DockerAIPuller();
    
    bool pull_model(const std::string& model_spec, const std::string& output_path = "");
    bool validate_gguf(const std::string& file_path);
    void set_max_connections(int count) { max_connections_ = count; }
    void set_max_retries(int count) { max_retries_ = count; }
    
private:
    int max_connections_;
    int max_retries_;
    
    std::string parse_model_spec(const std::string& model_spec);
    std::string get_registry_url(const std::string& model_name);
    std::string get_download_url(const std::string& registry_url);
    size_t get_content_length(const std::string& url);
    bool download_file(const std::string& url, const std::string& output_path, size_t expected_size);
    bool resume_download(const std::string& url, const std::string& output_path, size_t current_size, size_t total_size);
    
    static size_t write_callback(void* contents, size_t size, size_t nmemb, void* userp);
    static int progress_callback(void* clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow);
};