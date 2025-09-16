#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <chrono>
#include <curl/curl.h>

class DockerPuller {
public:
    struct DownloadConfig {
        std::string model_name;         // e.g., "ai/smollm2:135M-Q4_0"
        std::string output_path;        // Output file path
        int max_connections = 1;        // Number of concurrent connections
        int max_retries = 3;            // Maximum retry attempts
        bool resume_download = true;    // Enable resumable downloads
        std::string registry_url = "https://registry-1.docker.io"; // Docker registry URL
    };

    struct DownloadProgress {
        size_t total_bytes = 0;
        size_t downloaded_bytes = 0;
        double download_speed = 0.0;    // bytes per second
        int active_connections = 0;
        std::chrono::steady_clock::time_point start_time;
    };

    using ProgressCallback = std::function<void(const DownloadProgress&)>;

    DockerPuller();
    ~DockerPuller();

    // Main download function
    bool download(const DownloadConfig& config, ProgressCallback progress_cb = nullptr);

    // Parse Docker AI model reference (e.g., "ai/smollm2:135M-Q4_0")
    static bool parseModelReference(const std::string& model_ref, 
                                  std::string& repository, 
                                  std::string& tag);

private:
    struct ManifestInfo {
        std::string digest;
        size_t size;
        std::string media_type;
    };

    struct DownloadRange {
        size_t start;
        size_t end;
        size_t downloaded;
        CURL* curl_handle;
        FILE* file_handle;
        int range_id;
    };

    // Docker registry operations
    bool authenticate(const std::string& repository);
    bool getManifest(const std::string& repository, const std::string& tag, ManifestInfo& manifest);
    bool downloadBlob(const std::string& digest, const std::string& output_path, 
                     const DownloadConfig& config, ProgressCallback progress_cb);

    // Multi-threaded download implementation
    bool downloadWithRanges(const std::string& download_url, const std::string& output_path,
                           size_t total_size, const DownloadConfig& config, 
                           ProgressCallback progress_cb);
    
    // Single connection download
    bool downloadSingleConnection(const std::string& download_url, const std::string& output_path,
                                size_t total_size, size_t existing_size, const DownloadConfig& config,
                                ProgressCallback progress_cb);
    
    // Multi-connection download
    bool downloadMultiConnection(const std::string& download_url, const std::string& output_path,
                               size_t total_size, size_t existing_size, const DownloadConfig& config,
                               ProgressCallback progress_cb);

    // Utility functions
    std::vector<DownloadRange> calculateRanges(size_t total_size, int num_connections);
    bool validatePartialFile(const std::string& file_path, size_t expected_size);
    size_t getExistingFileSize(const std::string& file_path);

    // Retry logic
    bool retryDownload(std::function<bool()> download_func, int max_retries);

    // CURL callbacks
    static size_t writeCallback(void* contents, size_t size, size_t nmemb, void* userp);
    static size_t fileWriteCallback(void* contents, size_t size, size_t nmemb, void* userp);
    static int progressCallback(void* clientp, curl_off_t dltotal, curl_off_t dlnow, 
                               curl_off_t ultotal, curl_off_t ulnow);

    // Member variables
    std::string auth_token_;
    std::string registry_url_;
    CURLM* multi_handle_;
    bool initialized_;
};