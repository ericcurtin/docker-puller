#pragma once

#include <string>
#include <vector>
#include <memory>
#include <curl/curl.h>

struct DownloadRange {
    size_t start;
    size_t end;
    size_t total_size;
    std::vector<char> data;
    size_t bytes_downloaded;
    int connection_id;
};

struct DownloadConfig {
    std::string url;
    std::string output_file;
    int max_connections = 1;
    int max_retries = 3;
    bool resume = true;
    bool verbose = false;
};

class DockerPuller {
public:
    DockerPuller(const DownloadConfig& config);
    ~DockerPuller();

    bool download();
    
private:
    DownloadConfig config_;
    CURLM* multi_handle_;
    std::vector<std::unique_ptr<DownloadRange>> ranges_;
    
    bool init_curl();
    void cleanup_curl();
    bool get_file_size(const std::string& url, size_t& size);
    bool setup_ranges(size_t total_size);
    bool download_with_ranges();
    bool merge_ranges_to_file();
    static size_t write_callback(void* contents, size_t size, size_t nmemb, DownloadRange* range);
    static int progress_callback(void* clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow);
    bool file_exists(const std::string& filename);
    size_t get_existing_file_size(const std::string& filename);
};