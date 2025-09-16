#ifndef DOCKER_PULLER_H
#define DOCKER_PULLER_H

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <curl/curl.h>

struct DownloadRange {
    size_t start;
    size_t end;
    size_t connection_id;
    std::string temp_filename;
    FILE* file;
    bool completed;
    size_t bytes_downloaded;
};

struct DownloadProgress {
    size_t total_size;
    size_t downloaded_bytes;
    double speed_mbps;
    int active_connections;
    std::string status;
};

class DockerPuller {
public:
    DockerPuller(int max_connections = 1, int max_retries = 3);
    ~DockerPuller();

    bool pull(const std::string& model_spec, const std::string& output_path);
    void set_progress_callback(std::function<void(const DownloadProgress&)> callback);

private:
    int max_connections_;
    int max_retries_;
    std::function<void(const DownloadProgress&)> progress_callback_;
    
    bool supports_range_requests(const std::string& url);
    size_t get_content_length(const std::string& url);
    bool download_with_ranges(const std::string& url, const std::string& output_path, size_t total_size);
    bool download_single_connection(const std::string& url, const std::string& output_path);
    bool merge_temp_files(const std::vector<DownloadRange>& ranges, const std::string& output_path);
    std::string get_download_url(const std::string& model_spec);
    bool file_exists(const std::string& path);
    size_t get_file_size(const std::string& path);
    
    static size_t write_callback(void* contents, size_t size, size_t nmemb, void* userp);
    static size_t header_callback(char* buffer, size_t size, size_t nitems, void* userdata);
    static int progress_callback_static(void* clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow);
};

#endif // DOCKER_PULLER_H