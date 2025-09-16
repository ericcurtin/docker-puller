#include "docker_puller.h"
#include "url_parser.h"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <thread>
#include <chrono>
#include <algorithm>
#include <cmath>

DockerPuller::DockerPuller(int max_connections, int max_retries) 
    : max_connections_(max_connections), max_retries_(max_retries) {
    curl_global_init(CURL_GLOBAL_DEFAULT);
}

DockerPuller::~DockerPuller() {
    curl_global_cleanup();
}

void DockerPuller::set_progress_callback(std::function<void(const DownloadProgress&)> callback) {
    progress_callback_ = callback;
}

bool DockerPuller::pull(const std::string& model_spec, const std::string& output_path) {
    std::cout << "Pulling Docker AI model: " << model_spec << std::endl;
    
    // Parse model specification
    ModelSpec spec = UrlParser::parse_model_spec(model_spec);
    if (!spec.is_valid()) {
        std::cerr << "Error: Invalid model specification: " << model_spec << std::endl;
        return false;
    }
    
    std::string download_url = get_download_url(model_spec);
    if (download_url.empty()) {
        std::cerr << "Error: Could not build download URL for model: " << model_spec << std::endl;
        return false;
    }
    
    std::cout << "Download URL: " << download_url << std::endl;
    
    // Check if file already exists and get its size for resume capability
    size_t existing_size = 0;
    if (file_exists(output_path)) {
        existing_size = get_file_size(output_path);
        std::cout << "Found existing file of size: " << existing_size << " bytes" << std::endl;
    }
    
    // Retry loop
    for (int attempt = 1; attempt <= max_retries_; ++attempt) {
        std::cout << "Attempt " << attempt << " of " << max_retries_ << std::endl;
        
        // Get content length
        size_t total_size = get_content_length(download_url);
        if (total_size == 0) {
            std::cerr << "Warning: Could not determine file size" << std::endl;
        }
        
        // Check if we can resume
        if (existing_size > 0 && existing_size < total_size) {
            std::cout << "Resuming download from byte " << existing_size << std::endl;
        } else if (existing_size >= total_size && total_size > 0) {
            std::cout << "File already complete" << std::endl;
            return true;
        }
        
        bool success = false;
        
        // Use range requests for multiple connections if supported
        if (max_connections_ > 1 && supports_range_requests(download_url)) {
            std::cout << "Using " << max_connections_ << " concurrent connections" << std::endl;
            success = download_with_ranges(download_url, output_path, total_size);
        } else {
            std::cout << "Using single connection download" << std::endl;
            success = download_single_connection(download_url, output_path);
        }
        
        if (success) {
            std::cout << "Download completed successfully" << std::endl;
            return true;
        }
        
        if (attempt < max_retries_) {
            std::cout << "Download failed, retrying in 2 seconds..." << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(2));
            existing_size = file_exists(output_path) ? get_file_size(output_path) : 0;
        }
    }
    
    std::cerr << "Download failed after " << max_retries_ << " attempts" << std::endl;
    return false;
}

std::string DockerPuller::get_download_url(const std::string& model_spec) {
    ModelSpec spec = UrlParser::parse_model_spec(model_spec);
    return UrlParser::build_download_url(spec);
}

bool DockerPuller::supports_range_requests(const std::string& url) {
    CURL* curl = curl_easy_init();
    if (!curl) return false;
    
    std::string headers;
    
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, header_callback);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &headers);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    
    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    
    if (res != CURLE_OK) {
        return false;
    }
    
    // Check for Accept-Ranges header
    return headers.find("Accept-Ranges: bytes") != std::string::npos;
}

size_t DockerPuller::get_content_length(const std::string& url) {
    CURL* curl = curl_easy_init();
    if (!curl) return 0;
    
    curl_off_t content_length = 0;
    
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    
    CURLcode res = curl_easy_perform(curl);
    
    if (res == CURLE_OK) {
        curl_easy_getinfo(curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD_T, &content_length);
    }
    
    curl_easy_cleanup(curl);
    return static_cast<size_t>(content_length);
}

bool DockerPuller::download_single_connection(const std::string& url, const std::string& output_path) {
    CURL* curl = curl_easy_init();
    if (!curl) return false;
    
    // Check for existing file to resume
    size_t resume_from = 0;
    if (file_exists(output_path)) {
        resume_from = get_file_size(output_path);
    }
    
    FILE* file = fopen(output_path.c_str(), resume_from > 0 ? "ab" : "wb");
    if (!file) {
        curl_easy_cleanup(curl);
        return false;
    }
    
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, file);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 0L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 30L);
    
    if (resume_from > 0) {
        curl_easy_setopt(curl, CURLOPT_RESUME_FROM_LARGE, static_cast<curl_off_t>(resume_from));
    }
    
    // Set progress callback
    if (progress_callback_) {
        curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, progress_callback_static);
        curl_easy_setopt(curl, CURLOPT_XFERINFODATA, this);
        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    }
    
    CURLcode res = curl_easy_perform(curl);
    
    fclose(file);
    curl_easy_cleanup(curl);
    
    return res == CURLE_OK;
}

bool DockerPuller::download_with_ranges(const std::string& url, const std::string& output_path, size_t total_size) {
    if (total_size == 0) {
        // Fall back to single connection if we can't determine size
        return download_single_connection(url, output_path);
    }
    
    // Calculate ranges for each connection
    std::vector<DownloadRange> ranges;
    size_t bytes_per_connection = total_size / max_connections_;
    
    for (int i = 0; i < max_connections_; ++i) {
        DownloadRange range;
        range.connection_id = i;
        range.start = i * bytes_per_connection;
        range.end = (i == max_connections_ - 1) ? total_size - 1 : (i + 1) * bytes_per_connection - 1;
        range.temp_filename = output_path + ".part" + std::to_string(i);
        range.completed = false;
        range.bytes_downloaded = 0;
        
        // Check for existing partial file
        if (file_exists(range.temp_filename)) {
            size_t existing_size = get_file_size(range.temp_filename);
            range.start += existing_size;
            range.bytes_downloaded = existing_size;
            if (range.start > range.end) {
                range.completed = true;
            }
        }
        
        ranges.push_back(range);
    }
    
    // Create curl multi handle
    CURLM* multi_handle = curl_multi_init();
    if (!multi_handle) return false;
    
    std::vector<CURL*> easy_handles;
    
    // Set up each connection
    for (auto& range : ranges) {
        if (range.completed) continue;
        
        CURL* curl = curl_easy_init();
        if (!curl) continue;
        
        range.file = fopen(range.temp_filename.c_str(), "ab");
        if (!range.file) {
            curl_easy_cleanup(curl);
            continue;
        }
        
        std::string range_header = std::to_string(range.start) + "-" + std::to_string(range.end);
        
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, range.file);
        curl_easy_setopt(curl, CURLOPT_RANGE, range_header.c_str());
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 0L);
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 30L);
        
        curl_multi_add_handle(multi_handle, curl);
        easy_handles.push_back(curl);
    }
    
    // Perform the downloads
    int running_handles;
    CURLMcode mc = curl_multi_perform(multi_handle, &running_handles);
    
    if (mc != CURLM_OK) {
        // Cleanup and fall back to single connection
        for (CURL* curl : easy_handles) {
            curl_multi_remove_handle(multi_handle, curl);
            curl_easy_cleanup(curl);
        }
        curl_multi_cleanup(multi_handle);
        
        for (auto& range : ranges) {
            if (range.file) fclose(range.file);
        }
        
        return download_single_connection(url, output_path);
    }
    
    // Wait for downloads to complete
    while (running_handles > 0) {
        fd_set fdread, fdwrite, fdexcep;
        int maxfd = -1;
        long curl_timeo = -1;
        
        FD_ZERO(&fdread);
        FD_ZERO(&fdwrite);
        FD_ZERO(&fdexcep);
        
        curl_multi_timeout(multi_handle, &curl_timeo);
        if (curl_timeo >= 0) {
            struct timeval timeout;
            timeout.tv_sec = curl_timeo / 1000;
            timeout.tv_usec = (curl_timeo % 1000) * 1000;
            
            curl_multi_fdset(multi_handle, &fdread, &fdwrite, &fdexcep, &maxfd);
            if (maxfd >= 0) {
                select(maxfd + 1, &fdread, &fdwrite, &fdexcep, &timeout);
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }
        
        curl_multi_perform(multi_handle, &running_handles);
    }
    
    // Cleanup curl handles
    for (CURL* curl : easy_handles) {
        curl_multi_remove_handle(multi_handle, curl);
        curl_easy_cleanup(curl);
    }
    curl_multi_cleanup(multi_handle);
    
    // Close temp files
    for (auto& range : ranges) {
        if (range.file) fclose(range.file);
    }
    
    // Merge temp files
    return merge_temp_files(ranges, output_path);
}

bool DockerPuller::merge_temp_files(const std::vector<DownloadRange>& ranges, const std::string& output_path) {
    std::ofstream output(output_path, std::ios::binary);
    if (!output) return false;
    
    for (const auto& range : ranges) {
        std::ifstream input(range.temp_filename, std::ios::binary);
        if (!input) {
            std::cerr << "Error: Cannot open temp file: " << range.temp_filename << std::endl;
            return false;
        }
        
        output << input.rdbuf();
        input.close();
        
        // Remove temp file
        std::filesystem::remove(range.temp_filename);
    }
    
    output.close();
    return true;
}

bool DockerPuller::file_exists(const std::string& path) {
    return std::filesystem::exists(path);
}

size_t DockerPuller::get_file_size(const std::string& path) {
    try {
        return std::filesystem::file_size(path);
    } catch (const std::filesystem::filesystem_error&) {
        return 0;
    }
}

size_t DockerPuller::write_callback(void* contents, size_t size, size_t nmemb, void* userp) {
    FILE* file = static_cast<FILE*>(userp);
    return fwrite(contents, size, nmemb, file);
}

size_t DockerPuller::header_callback(char* buffer, size_t size, size_t nitems, void* userdata) {
    size_t realsize = size * nitems;
    std::string* headers = static_cast<std::string*>(userdata);
    headers->append(buffer, realsize);
    return realsize;
}

int DockerPuller::progress_callback_static(void* clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t /*ultotal*/, curl_off_t /*ulnow*/) {
    DockerPuller* puller = static_cast<DockerPuller*>(clientp);
    
    if (puller->progress_callback_ && dltotal > 0) {
        DownloadProgress progress;
        progress.total_size = static_cast<size_t>(dltotal);
        progress.downloaded_bytes = static_cast<size_t>(dlnow);
        progress.speed_mbps = 0.0; // Would need timing calculation
        progress.active_connections = 1;
        progress.status = "Downloading";
        
        puller->progress_callback_(progress);
    }
    
    return 0;
}