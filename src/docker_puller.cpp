#include "docker_puller.hpp"
#include "gguf_validator.hpp"
#include <nlohmann/json.hpp>
#include <iostream>
#include <fstream>
#include <sstream>
#include <thread>
#include <algorithm>
#include <regex>
#include <sys/stat.h>
#include <unistd.h>

using json = nlohmann::json;

DockerPuller::DockerPuller() : multi_handle_(nullptr), initialized_(false) {
    if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK) {
        std::cerr << "Failed to initialize libcurl" << std::endl;
        return;
    }
    
    multi_handle_ = curl_multi_init();
    if (!multi_handle_) {
        std::cerr << "Failed to initialize libcurl multi handle" << std::endl;
        curl_global_cleanup();
        return;
    }
    
    initialized_ = true;
}

DockerPuller::~DockerPuller() {
    if (multi_handle_) {
        curl_multi_cleanup(multi_handle_);
    }
    if (initialized_) {
        curl_global_cleanup();
    }
}

bool DockerPuller::download(const DownloadConfig& config, ProgressCallback progress_cb) {
    if (!initialized_) {
        std::cerr << "DockerPuller not properly initialized" << std::endl;
        return false;
    }
    
    // Parse model reference
    std::string repository, tag;
    if (!parseModelReference(config.model_name, repository, tag)) {
        std::cerr << "Invalid model reference: " << config.model_name << std::endl;
        return false;
    }
    
    registry_url_ = config.registry_url;
    
    // Download with retry logic
    return retryDownload([&]() {
        // Authenticate
        if (!authenticate(repository)) {
            std::cerr << "Authentication failed" << std::endl;
            return false;
        }
        
        // Get manifest
        ManifestInfo manifest;
        if (!getManifest(repository, tag, manifest)) {
            std::cerr << "Failed to get manifest" << std::endl;
            return false;
        }
        
        std::cout << "Found blob: " << manifest.digest << " (" << manifest.size << " bytes)" << std::endl;
        
        // Download blob
        if (!downloadBlob(manifest.digest, config.output_path, config, progress_cb)) {
            std::cerr << "Failed to download blob" << std::endl;
            return false;
        }
        
        // Validate GGUF file
        if (!GGUFValidator::validate(config.output_path)) {
            std::cerr << "Downloaded file is not a valid GGUF file" << std::endl;
            return false;
        }
        
        return true;
    }, config.max_retries);
}

bool DockerPuller::parseModelReference(const std::string& model_ref, 
                                      std::string& repository, 
                                      std::string& tag) {
    // Parse format like "ai/smollm2:135M-Q4_0"
    std::regex pattern(R"(^([^:]+):(.+)$)");
    std::smatch matches;
    
    if (std::regex_match(model_ref, matches, pattern)) {
        repository = matches[1].str();
        tag = matches[2].str();
        return true;
    }
    
    return false;
}

bool DockerPuller::authenticate(const std::string& repository) {
    // For public repositories, we typically need to get a token from Docker Hub auth service
    CURL* curl = curl_easy_init();
    if (!curl) {
        return false;
    }
    
    std::string response;
    std::string auth_url = "https://auth.docker.io/token?service=registry.docker.io&scope=repository:" + repository + ":pull";
    
    curl_easy_setopt(curl, CURLOPT_URL, auth_url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    
    CURLcode res = curl_easy_perform(curl);
    long response_code;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
    curl_easy_cleanup(curl);
    
    if (res != CURLE_OK || response_code != 200) {
        std::cerr << "Authentication request failed: " << curl_easy_strerror(res) << std::endl;
        return false;
    }
    
    try {
        json auth_response = json::parse(response);
        if (auth_response.contains("token")) {
            auth_token_ = auth_response["token"];
            return true;
        }
    } catch (const json::exception& e) {
        std::cerr << "Failed to parse auth response: " << e.what() << std::endl;
    }
    
    return false;
}

bool DockerPuller::getManifest(const std::string& repository, const std::string& tag, ManifestInfo& manifest) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        return false;
    }
    
    std::string response;
    std::string manifest_url = registry_url_ + "/v2/" + repository + "/manifests/" + tag;
    
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Accept: application/vnd.docker.ai.gguf.v3, application/vnd.docker.distribution.manifest.v2+json");
    if (!auth_token_.empty()) {
        std::string auth_header = "Authorization: Bearer " + auth_token_;
        headers = curl_slist_append(headers, auth_header.c_str());
    }
    
    curl_easy_setopt(curl, CURLOPT_URL, manifest_url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    
    CURLcode res = curl_easy_perform(curl);
    long response_code;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
    
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    
    if (res != CURLE_OK || response_code != 200) {
        std::cerr << "Manifest request failed: " << curl_easy_strerror(res) 
                  << " (HTTP " << response_code << ")" << std::endl;
        return false;
    }
    
    try {
        json manifest_json = json::parse(response);
        
        // Handle different manifest formats
        if (manifest_json.contains("config")) {
            // Standard Docker manifest v2
            manifest.digest = manifest_json["config"]["digest"];
            manifest.size = manifest_json["config"]["size"];
            manifest.media_type = manifest_json["config"]["mediaType"];
        } else if (manifest_json.contains("layers") && !manifest_json["layers"].empty()) {
            // Use the first layer if config is not available
            auto layer = manifest_json["layers"][0];
            manifest.digest = layer["digest"];
            manifest.size = layer["size"];
            manifest.media_type = layer.value("mediaType", "application/octet-stream");
        } else {
            std::cerr << "Unexpected manifest format" << std::endl;
            return false;
        }
        
        return true;
    } catch (const json::exception& e) {
        std::cerr << "Failed to parse manifest: " << e.what() << std::endl;
        return false;
    }
}

bool DockerPuller::downloadBlob(const std::string& digest, const std::string& output_path, 
                               const DownloadConfig& config, ProgressCallback progress_cb) {
    std::string blob_url = registry_url_ + "/v2/" + config.model_name.substr(0, config.model_name.find(':')) + "/blobs/" + digest;
    
    // Get blob size first
    CURL* curl = curl_easy_init();
    if (!curl) {
        return false;
    }
    
    struct curl_slist* headers = nullptr;
    if (!auth_token_.empty()) {
        std::string auth_header = "Authorization: Bearer " + auth_token_;
        headers = curl_slist_append(headers, auth_header.c_str());
    }
    
    curl_easy_setopt(curl, CURLOPT_URL, blob_url.c_str());
    curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);  // HEAD request
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    
    CURLcode res = curl_easy_perform(curl);
    long response_code;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
    
    curl_off_t content_length;
    curl_easy_getinfo(curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD_T, &content_length);
    
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    
    if (res != CURLE_OK || response_code != 200) {
        std::cerr << "Blob HEAD request failed: " << curl_easy_strerror(res) 
                  << " (HTTP " << response_code << ")" << std::endl;
        return false;
    }
    
    size_t total_size = content_length > 0 ? content_length : 0;
    std::cout << "Blob size: " << total_size << " bytes" << std::endl;
    
    // Download with multi-connection support
    return downloadWithRanges(blob_url, output_path, total_size, config, progress_cb);
}

bool DockerPuller::downloadWithRanges(const std::string& download_url, const std::string& output_path,
                                     size_t total_size, const DownloadConfig& config, 
                                     ProgressCallback progress_cb) {
    // Check if file exists and get existing size for resume functionality
    size_t existing_size = 0;
    if (config.resume_download) {
        existing_size = getExistingFileSize(output_path);
        if (existing_size >= total_size) {
            std::cout << "File already downloaded completely" << std::endl;
            return true;
        }
    }
    
    // Calculate ranges for multi-connection download
    size_t remaining_size = total_size - existing_size;
    std::vector<DownloadRange> ranges = calculateRanges(remaining_size, config.max_connections);
    
    // Adjust ranges to account for existing data
    for (auto& range : ranges) {
        range.start += existing_size;
        range.end += existing_size;
    }
    
    // Open file for writing (append mode for resume)
    FILE* output_file = fopen(output_path.c_str(), existing_size > 0 ? "r+b" : "wb");
    if (!output_file) {
        std::cerr << "Cannot open output file: " << output_path << std::endl;
        return false;
    }
    
    // Seek to end if resuming
    if (existing_size > 0) {
        fseek(output_file, 0, SEEK_END);
    }
    
    // Setup progress tracking
    DownloadProgress progress;
    progress.total_bytes = total_size;
    progress.downloaded_bytes = existing_size;
    progress.start_time = std::chrono::steady_clock::now();
    progress.active_connections = ranges.size();
    
    // Create CURL handles for each range
    std::vector<CURL*> curl_handles;
    for (size_t i = 0; i < ranges.size(); ++i) {
        CURL* curl = curl_easy_init();
        if (!curl) {
            fclose(output_file);
            return false;
        }
        
        ranges[i].curl_handle = curl;
        ranges[i].file_handle = output_file;
        ranges[i].range_id = i;
        
        // Setup range request
        std::string range_header = "Range: bytes=" + std::to_string(ranges[i].start) + "-" + std::to_string(ranges[i].end);
        
        struct curl_slist* headers = nullptr;
        headers = curl_slist_append(headers, range_header.c_str());
        if (!auth_token_.empty()) {
            std::string auth_header = "Authorization: Bearer " + auth_token_;
            headers = curl_slist_append(headers, auth_header.c_str());
        }
        
        curl_easy_setopt(curl, CURLOPT_URL, download_url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &ranges[i]);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 300L);  // 5 minute timeout
        curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 1024L);  // 1KB/s minimum
        curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, 30L);     // for 30 seconds
        
        curl_multi_add_handle(multi_handle_, curl);
        curl_handles.push_back(curl);
    }
    
    // Start download
    int running_handles;
    curl_multi_perform(multi_handle_, &running_handles);
    
    // Monitor progress
    while (running_handles > 0) {
        CURLMcode mc = curl_multi_wait(multi_handle_, nullptr, 0, 1000, nullptr);
        if (mc != CURLM_OK) {
            std::cerr << "curl_multi_wait failed: " << curl_multi_strerror(mc) << std::endl;
            break;
        }
        
        curl_multi_perform(multi_handle_, &running_handles);
        
        // Update progress
        if (progress_cb) {
            size_t total_downloaded = existing_size;
            for (const auto& range : ranges) {
                total_downloaded += range.downloaded;
            }
            progress.downloaded_bytes = total_downloaded;
            
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - progress.start_time).count();
            if (elapsed > 0) {
                progress.download_speed = (double)(total_downloaded - existing_size) / elapsed;
            }
            
            progress_cb(progress);
        }
        
        // Check for completed transfers
        CURLMsg* msg;
        int msgs_left;
        while ((msg = curl_multi_info_read(multi_handle_, &msgs_left))) {
            if (msg->msg == CURLMSG_DONE) {
                CURL* easy_handle = msg->easy_handle;
                long response_code;
                curl_easy_getinfo(easy_handle, CURLINFO_RESPONSE_CODE, &response_code);
                
                if (msg->data.result != CURLE_OK || (response_code != 200 && response_code != 206)) {
                    std::cerr << "Transfer failed: " << curl_easy_strerror(msg->data.result) 
                              << " (HTTP " << response_code << ")" << std::endl;
                }
            }
        }
    }
    
    // Cleanup
    for (CURL* curl : curl_handles) {
        curl_multi_remove_handle(multi_handle_, curl);
        curl_easy_cleanup(curl);
    }
    
    fclose(output_file);
    
    // Verify download completion
    return validatePartialFile(output_path, total_size);
}

std::vector<DockerPuller::DownloadRange> DockerPuller::calculateRanges(size_t total_size, int num_connections) {
    std::vector<DownloadRange> ranges;
    
    if (total_size == 0 || num_connections <= 0) {
        return ranges;
    }
    
    size_t chunk_size = total_size / num_connections;
    size_t remainder = total_size % num_connections;
    
    size_t current_start = 0;
    for (int i = 0; i < num_connections; ++i) {
        DownloadRange range;
        range.start = current_start;
        range.end = current_start + chunk_size - 1;
        
        // Add remainder to last chunk
        if (i == num_connections - 1) {
            range.end += remainder;
        }
        
        range.downloaded = 0;
        range.curl_handle = nullptr;
        range.file_handle = nullptr;
        range.range_id = i;
        
        ranges.push_back(range);
        current_start = range.end + 1;
    }
    
    return ranges;
}

bool DockerPuller::validatePartialFile(const std::string& file_path, size_t expected_size) {
    std::ifstream file(file_path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        return false;
    }
    
    size_t actual_size = file.tellg();
    return actual_size == expected_size;
}

size_t DockerPuller::getExistingFileSize(const std::string& file_path) {
    struct stat st;
    if (stat(file_path.c_str(), &st) == 0) {
        return st.st_size;
    }
    return 0;
}

bool DockerPuller::retryDownload(std::function<bool()> download_func, int max_retries) {
    for (int attempt = 0; attempt <= max_retries; ++attempt) {
        if (attempt > 0) {
            std::cout << "Retry attempt " << attempt << "/" << max_retries << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(2 * attempt));  // Exponential backoff
        }
        
        if (download_func()) {
            return true;
        }
        
        if (attempt < max_retries) {
            std::cout << "Download failed, retrying..." << std::endl;
        }
    }
    
    std::cout << "Download failed after " << max_retries << " retries" << std::endl;
    return false;
}

size_t DockerPuller::writeCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t real_size = size * nmemb;
    
    // Check if this is a string response (for auth/manifest requests)
    if (auto str_ptr = static_cast<std::string*>(userp)) {
        str_ptr->append(static_cast<char*>(contents), real_size);
        return real_size;
    }
    
    // Otherwise, it's a download range
    auto range = static_cast<DownloadRange*>(userp);
    if (range && range->file_handle) {
        // Calculate file position for this range
        size_t file_pos = range->start + range->downloaded;
        
        // Seek to correct position and write
        fseek(range->file_handle, file_pos, SEEK_SET);
        size_t written = fwrite(contents, 1, real_size, range->file_handle);
        
        range->downloaded += written;
        return written;
    }
    
    return real_size;
}

int DockerPuller::progressCallback(void* /*clientp*/, curl_off_t /*dltotal*/, curl_off_t /*dlnow*/, 
                                  curl_off_t /*ultotal*/, curl_off_t /*ulnow*/) {
    // This callback can be used for individual transfer progress
    // Currently, we handle progress in the main download loop
    return 0;
}