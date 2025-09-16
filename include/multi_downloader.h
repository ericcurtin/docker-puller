#pragma once

#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <curl/curl.h>

/**
 * Download progress callback
 */
using ProgressCallback = std::function<void(size_t downloaded, size_t total)>;

/**
 * Download chunk for multi-connection downloading
 */
struct DownloadChunk {
    size_t start;
    size_t end;
    std::string data;
    bool completed = false;
    CURL* curl_handle = nullptr;
    int connection_id;
};

/**
 * Multi-connection downloader using libcurl multi interface
 */
class MultiDownloader {
public:
    explicit MultiDownloader(int max_connections = 1);
    ~MultiDownloader();
    
    /**
     * Download file with support for resumable downloads and retries
     */
    bool download_file(const std::string& url, 
                      const std::string& output_path,
                      const std::string& auth_token = "",
                      int max_retries = 3);
    
    /**
     * Set progress callback
     */
    void set_progress_callback(ProgressCallback callback);
    
    /**
     * Get file size from server
     */
    size_t get_content_length(const std::string& url, const std::string& auth_token = "");
    
private:
    int max_connections_;
    CURLM* multi_handle_;
    ProgressCallback progress_callback_;
    
    // Multi-connection download methods
    bool download_with_chunks(const std::string& url, 
                             const std::string& output_path,
                             const std::string& auth_token,
                             size_t file_size);
    
    bool download_single_connection(const std::string& url,
                                   const std::string& output_path, 
                                   const std::string& auth_token);
    
    // Resume support
    size_t get_existing_file_size(const std::string& output_path);
    bool supports_range_requests(const std::string& url, const std::string& auth_token);
    
    // Curl setup helpers
    CURL* setup_curl_handle(const std::string& url, const std::string& auth_token);
    void cleanup_curl_handle(CURL* curl);
    
    // Static curl callbacks
    static size_t write_data_callback(void* contents, size_t size, size_t nmemb, void* userp);
    static size_t header_callback(char* buffer, size_t size, size_t nitems, void* userdata);
    static int progress_callback(void* clientp, curl_off_t dltotal, curl_off_t dlnow, 
                                curl_off_t ultotal, curl_off_t ulnow);
};