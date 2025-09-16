#include "multi_downloader.h"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <algorithm>
#include <thread>
#include <chrono>

MultiDownloader::MultiDownloader(int max_connections) 
    : max_connections_(max_connections), multi_handle_(nullptr) {
    curl_global_init(CURL_GLOBAL_DEFAULT);
    multi_handle_ = curl_multi_init();
    if (!multi_handle_) {
        throw std::runtime_error("Failed to initialize curl multi handle");
    }
}

MultiDownloader::~MultiDownloader() {
    if (multi_handle_) {
        curl_multi_cleanup(multi_handle_);
    }
    curl_global_cleanup();
}

void MultiDownloader::set_progress_callback(ProgressCallback callback) {
    progress_callback_ = callback;
}

size_t MultiDownloader::get_content_length(const std::string& url, const std::string& auth_token) {
    CURL* curl = setup_curl_handle(url, auth_token);
    if (!curl) return 0;
    
    // Use HEAD request to get content length
    curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    
    CURLcode res = curl_easy_perform(curl);
    
    curl_off_t content_length = 0;
    if (res == CURLE_OK) {
        curl_easy_getinfo(curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD_T, &content_length);
    }
    
    cleanup_curl_handle(curl);
    return static_cast<size_t>(content_length);
}

bool MultiDownloader::supports_range_requests(const std::string& url, const std::string& auth_token) {
    CURL* curl = setup_curl_handle(url, auth_token);
    if (!curl) return false;
    
    // Use HEAD request to check for Accept-Ranges header
    curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    
    std::string headers;
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &headers);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, header_callback);
    
    CURLcode res = curl_easy_perform(curl);
    
    long response_code;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
    
    cleanup_curl_handle(curl);
    
    if (res != CURLE_OK || response_code >= 400) {
        std::cout << "Range request test failed, using single connection" << std::endl;
        return false;
    }
    
    // Convert to lowercase and check for Accept-Ranges: bytes
    std::transform(headers.begin(), headers.end(), headers.begin(), ::tolower);
    bool supports_ranges = headers.find("accept-ranges: bytes") != std::string::npos;
    
    if (!supports_ranges) {
        std::cout << "Server doesn't advertise range support" << std::endl;
    }
    
    return supports_ranges;
}

size_t MultiDownloader::get_existing_file_size(const std::string& output_path) {
    try {
        if (std::filesystem::exists(output_path)) {
            return std::filesystem::file_size(output_path);
        }
    } catch (const std::exception& e) {
        std::cerr << "Error checking file size: " << e.what() << std::endl;
    }
    return 0;
}

bool MultiDownloader::download_file(const std::string& url, 
                                   const std::string& output_path,
                                   const std::string& auth_token,
                                   int max_retries) {
    for (int retry = 0; retry <= max_retries; ++retry) {
        if (retry > 0) {
            std::cout << "Retry attempt " << retry << "/" << max_retries << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(1 << (retry - 1))); // Exponential backoff
        }
        
        size_t file_size = get_content_length(url, auth_token);
        if (file_size == 0) {
            std::cerr << "Failed to get content length" << std::endl;
            continue;
        }
        
        std::cout << "File size: " << file_size << " bytes" << std::endl;
        
        // Check if we can resume
        size_t existing_size = get_existing_file_size(output_path);
        if (existing_size > 0 && existing_size < file_size) {
            std::cout << "Resuming download from " << existing_size << " bytes" << std::endl;
        }
        
        bool success = false;
        // For now, use single connection due to Docker registry limitations
        // Multi-connection support can be enabled for other registries that properly support ranges
        if (false && max_connections_ > 1 && supports_range_requests(url, auth_token) && file_size > 1024 * 1024) {
            // Use multi-connection download for large files that support ranges
            std::cout << "Using multi-connection download (" << max_connections_ << " connections)" << std::endl;
            success = download_with_chunks(url, output_path, auth_token, file_size);
        } else {
            // Use single connection download
            if (max_connections_ > 1) {
                std::cout << "Using single connection (Docker registry doesn't support range requests properly)" << std::endl;
            }
            success = download_single_connection(url, output_path, auth_token);
        }
        
        if (success) {
            std::cout << "Download completed successfully" << std::endl;
            return true;
        }
        
        std::cerr << "Download failed" << std::endl;
    }
    
    std::cerr << "Download failed after " << max_retries << " retries" << std::endl;
    return false;
}

bool MultiDownloader::download_single_connection(const std::string& url,
                                                const std::string& output_path,
                                                const std::string& auth_token) {
    CURL* curl = setup_curl_handle(url, auth_token);
    if (!curl) return false;
    
    // Check if we can resume
    size_t existing_size = get_existing_file_size(output_path);
    std::ofstream file;
    
    if (existing_size > 0) {
        // Resume download
        file.open(output_path, std::ios::binary | std::ios::app);
        curl_easy_setopt(curl, CURLOPT_RESUME_FROM, existing_size);
    } else {
        // Start fresh download
        file.open(output_path, std::ios::binary);
    }
    
    if (!file.is_open()) {
        std::cerr << "Failed to open output file: " << output_path << std::endl;
        cleanup_curl_handle(curl);
        return false;
    }
    
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &file);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data_callback);
    
    // Set progress callback
    if (progress_callback_) {
        curl_easy_setopt(curl, CURLOPT_PROGRESSDATA, this);
        curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, progress_callback);
        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    }
    
    CURLcode res = curl_easy_perform(curl);
    file.close();
    
    bool success = (res == CURLE_OK);
    if (!success) {
        std::cerr << "Curl error: " << curl_easy_strerror(res) << std::endl;
    }
    
    cleanup_curl_handle(curl);
    return success;
}

bool MultiDownloader::download_with_chunks(const std::string& url,
                                          const std::string& output_path,
                                          const std::string& auth_token,
                                          size_t file_size) {
    // Calculate chunk size
    size_t chunk_size = file_size / max_connections_;
    std::vector<DownloadChunk> chunks;
    
    // Create chunks
    for (int i = 0; i < max_connections_; ++i) {
        DownloadChunk chunk;
        chunk.start = i * chunk_size;
        chunk.end = (i == max_connections_ - 1) ? file_size - 1 : (i + 1) * chunk_size - 1;
        chunk.connection_id = i;
        chunks.push_back(chunk);
    }
    
    // Setup curl handles for each chunk
    for (auto& chunk : chunks) {
        chunk.curl_handle = setup_curl_handle(url, auth_token);
        if (!chunk.curl_handle) {
            // Cleanup on failure
            for (auto& c : chunks) {
                if (c.curl_handle) {
                    cleanup_curl_handle(c.curl_handle);
                }
            }
            return false;
        }
        
        // Set range
        std::string range = std::to_string(chunk.start) + "-" + std::to_string(chunk.end);
        curl_easy_setopt(chunk.curl_handle, CURLOPT_RANGE, range.c_str());
        curl_easy_setopt(chunk.curl_handle, CURLOPT_WRITEDATA, &chunk.data);
        curl_easy_setopt(chunk.curl_handle, CURLOPT_WRITEFUNCTION, write_data_callback);
        
        curl_multi_add_handle(multi_handle_, chunk.curl_handle);
    }
    
    // Perform multi download
    int running_handles;
    curl_multi_perform(multi_handle_, &running_handles);
    
    while (running_handles > 0) {
        fd_set fdread, fdwrite, fdexcep;
        int maxfd = -1;
        long curl_timeo = -1;
        
        FD_ZERO(&fdread);
        FD_ZERO(&fdwrite);
        FD_ZERO(&fdexcep);
        
        curl_multi_timeout(multi_handle_, &curl_timeo);
        if (curl_timeo >= 0) {
            struct timeval timeout;
            timeout.tv_sec = curl_timeo / 1000;
            timeout.tv_usec = (curl_timeo % 1000) * 1000;
            
            curl_multi_fdset(multi_handle_, &fdread, &fdwrite, &fdexcep, &maxfd);
            
            if (maxfd >= 0) {
                select(maxfd + 1, &fdread, &fdwrite, &fdexcep, &timeout);
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }
        
        curl_multi_perform(multi_handle_, &running_handles);
    }
    
    // Check results and write to file
    bool success = true;
    std::ofstream output_file(output_path, std::ios::binary);
    if (!output_file.is_open()) {
        std::cerr << "Failed to open output file: " << output_path << std::endl;
        success = false;
    } else {
        for (auto& chunk : chunks) {
            long response_code;
            curl_easy_getinfo(chunk.curl_handle, CURLINFO_RESPONSE_CODE, &response_code);
            
            if (response_code >= 200 && response_code < 300) {
                output_file.write(chunk.data.data(), chunk.data.size());
            } else {
                std::cerr << "Chunk " << chunk.connection_id << " failed with response code: " << response_code << std::endl;
                success = false;
            }
        }
        output_file.close();
    }
    
    // Cleanup
    for (auto& chunk : chunks) {
        curl_multi_remove_handle(multi_handle_, chunk.curl_handle);
        cleanup_curl_handle(chunk.curl_handle);
    }
    
    return success;
}

CURL* MultiDownloader::setup_curl_handle(const std::string& url, const std::string& auth_token) {
    CURL* curl = curl_easy_init();
    if (!curl) return nullptr;
    
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "docker-ai-puller/1.0");
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 300L); // 5 minutes timeout
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 30L); // 30 seconds connect timeout
    
    // Set authentication if provided
    if (!auth_token.empty()) {
        std::string auth_header = "Authorization: Bearer " + auth_token;
        struct curl_slist* headers = curl_slist_append(nullptr, auth_header.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    }
    
    return curl;
}

void MultiDownloader::cleanup_curl_handle(CURL* curl) {
    if (curl) {
        curl_easy_cleanup(curl);
    }
}

size_t MultiDownloader::write_data_callback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t total_size = size * nmemb;
    
    if (auto* file = static_cast<std::ofstream*>(userp)) {
        file->write(static_cast<const char*>(contents), total_size);
        return file->good() ? total_size : 0;
    } else if (auto* str = static_cast<std::string*>(userp)) {
        str->append(static_cast<const char*>(contents), total_size);
        return total_size;
    }
    
    return 0;
}

size_t MultiDownloader::header_callback(char* buffer, size_t size, size_t nitems, void* userdata) {
    size_t total_size = size * nitems;
    if (auto* headers = static_cast<std::string*>(userdata)) {
        headers->append(buffer, total_size);
    }
    return total_size;
}

int MultiDownloader::progress_callback(void* clientp, curl_off_t dltotal, curl_off_t dlnow,
                                      curl_off_t ultotal, curl_off_t ulnow) {
    if (auto* downloader = static_cast<MultiDownloader*>(clientp)) {
        if (downloader->progress_callback_ && dltotal > 0) {
            downloader->progress_callback_(static_cast<size_t>(dlnow), static_cast<size_t>(dltotal));
        }
    }
    return 0;
}