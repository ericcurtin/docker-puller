#pragma once

#include <string>
#include <vector>
#include <functional>
#include <memory>

struct DownloadProgress {
    size_t total_bytes;
    size_t downloaded_bytes;
    double speed_bytes_per_sec;
    int active_connections;
};

using ProgressCallback = std::function<void(const DownloadProgress&)>;

class MultiConnectionDownloader {
public:
    MultiConnectionDownloader(int max_connections = 1, int max_retries = 3);
    ~MultiConnectionDownloader();
    
    bool download(const std::string& url, const std::string& output_path,
                  ProgressCallback progress_cb = nullptr,
                  const std::vector<std::string>& headers = {});
    
    void set_max_connections(int count);
    void set_max_retries(int count);
    
private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};