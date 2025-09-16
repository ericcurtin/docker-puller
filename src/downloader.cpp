#include "downloader.h"
#include "utils.h"

#include <curl/curl.h>
#include <fstream>
#include <thread>
#include <chrono>
#include <algorithm>
#include <cstring>
#include <vector>
#include <memory>

struct DownloadChunk {
    size_t start;
    size_t end;
    int attempt;
    bool completed;
    std::string temp_file;
    FILE* file_handle;
    
    DownloadChunk(size_t s, size_t e) : start(s), end(e), attempt(0), completed(false), file_handle(nullptr) {}
};

struct WriteData {
    DownloadChunk* chunk;
    void* downloader; // Use void* instead of specific type
};

class MultiConnectionDownloader::Impl {
public:
    Impl(int max_connections, int max_retries) 
        : max_connections_(max_connections), max_retries_(max_retries), 
          total_size_(0), downloaded_bytes_(0) {
        curl_global_init(CURL_GLOBAL_DEFAULT);
        multi_handle_ = curl_multi_init();
        if (!multi_handle_) {
            throw std::runtime_error("Failed to initialize CURL multi handle");
        }
    }
    
    ~Impl() {
        // Close any open file handles
        for (auto& chunk : chunks_) {
            if (chunk && chunk->file_handle) {
                fclose(chunk->file_handle);
                chunk->file_handle = nullptr;
            }
        }
        
        if (multi_handle_) {
            curl_multi_cleanup(multi_handle_);
        }
        curl_global_cleanup();
    }
    
    bool download(const std::string& url, const std::string& output_path, 
                  ProgressCallback progress_cb, const std::vector<std::string>& headers) {
        url_ = url;
        output_path_ = output_path;
        progress_callback_ = progress_cb;
        headers_ = headers;
        downloaded_bytes_ = 0;
        
        // Check if file already exists and get resume position
        size_t resume_size = 0;
        if (utils::file_exists(output_path)) {
            resume_size = utils::get_file_size(output_path);
        }
        
        // Get file size
        if (!get_file_size()) {
            return false;
        }
        
        if (resume_size >= total_size_) {
            // File already completely downloaded
            downloaded_bytes_ = total_size_;
            return true;
        }
        
        downloaded_bytes_ = resume_size;
        
        // Create chunks
        create_chunks(resume_size);
        
        // Download chunks
        return download_chunks();
    }
    
    void set_max_connections(int count) {
        max_connections_ = std::max(1, std::min(16, count));
    }
    
    void set_max_retries(int count) {
        max_retries_ = std::max(0, std::min(10, count));
    }

private:
    static size_t write_callback(void* contents, size_t size, size_t nmemb, WriteData* data) {
        size_t total_size = size * nmemb;
        
        if (data->chunk->file_handle) {
            size_t written = fwrite(contents, 1, total_size, data->chunk->file_handle);
            // Update downloaded bytes in the downloader instance
            static_cast<MultiConnectionDownloader::Impl*>(data->downloader)->downloaded_bytes_ += written;
            return written;
        }
        
        return 0;
    }
    
    static size_t header_callback(char* /*buffer*/, size_t size, size_t nitems, void* /*userdata*/) {
        return size * nitems;
    }
    
    bool get_file_size() {
        CURL* curl = curl_easy_init();
        if (!curl) {
            return false;
        }
        
        curl_easy_setopt(curl, CURLOPT_URL, url_.c_str());
        curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "docker-puller/1.0");
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
        
        // Add headers if provided
        struct curl_slist* header_list = nullptr;
        for (const auto& header : headers_) {
            header_list = curl_slist_append(header_list, header.c_str());
        }
        if (header_list) {
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header_list);
        }
        
        CURLcode res = curl_easy_perform(curl);
        
        if (res == CURLE_OK) {
            curl_off_t content_length;
            curl_easy_getinfo(curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD_T, &content_length);
            if (content_length > 0) {
                total_size_ = (size_t)content_length;
            }
        }
        
        if (header_list) {
            curl_slist_free_all(header_list);
        }
        
        curl_easy_cleanup(curl);
        return res == CURLE_OK && total_size_ > 0;
    }
    
    void create_chunks(size_t resume_offset) {
        chunks_.clear();
        
        if (total_size_ <= resume_offset) {
            return;
        }
        
        size_t remaining_size = total_size_ - resume_offset;
        size_t chunk_size = remaining_size / max_connections_;
        
        if (chunk_size < 1024 * 1024) { // Minimum 1MB per chunk
            chunk_size = remaining_size;
            max_connections_ = 1;
        }
        
        for (int i = 0; i < max_connections_; ++i) {
            size_t start = resume_offset + i * chunk_size;
            size_t end = (i == max_connections_ - 1) ? total_size_ - 1 : start + chunk_size - 1;
            
            if (start >= total_size_) break;
            
            auto chunk = std::make_unique<DownloadChunk>(start, end);
            chunk->temp_file = utils::get_temp_filename(output_path_);
            chunks_.push_back(std::move(chunk));
        }
    }
    
    bool download_chunks() {
        int active_transfers = 0;
        std::vector<CURL*> easy_handles;
        std::vector<std::unique_ptr<WriteData>> write_data_list;
        
        // Create and add easy handles
        for (auto& chunk : chunks_) {
            if (chunk->completed) continue;
            
            CURL* easy = curl_easy_init();
            if (!easy) continue;
            
            // Open temp file for this chunk
            chunk->file_handle = fopen(chunk->temp_file.c_str(), "wb");
            if (!chunk->file_handle) {
                curl_easy_cleanup(easy);
                continue;
            }
            
            auto write_data = std::make_unique<WriteData>();
            write_data->chunk = chunk.get();
            write_data->downloader = this;
            
            std::string range = std::to_string(chunk->start) + "-" + std::to_string(chunk->end);
            
            curl_easy_setopt(easy, CURLOPT_URL, url_.c_str());
            curl_easy_setopt(easy, CURLOPT_RANGE, range.c_str());
            curl_easy_setopt(easy, CURLOPT_WRITEFUNCTION, write_callback);
            curl_easy_setopt(easy, CURLOPT_WRITEDATA, write_data.get());
            curl_easy_setopt(easy, CURLOPT_HEADERFUNCTION, header_callback);
            curl_easy_setopt(easy, CURLOPT_FOLLOWLOCATION, 1L);
            curl_easy_setopt(easy, CURLOPT_USERAGENT, "docker-puller/1.0");
            curl_easy_setopt(easy, CURLOPT_TIMEOUT, 300L);
            curl_easy_setopt(easy, CURLOPT_LOW_SPEED_LIMIT, 1024L);
            curl_easy_setopt(easy, CURLOPT_LOW_SPEED_TIME, 60L);
            
            // Add headers if provided
            struct curl_slist* header_list = nullptr;
            for (const auto& header : headers_) {
                header_list = curl_slist_append(header_list, header.c_str());
            }
            if (header_list) {
                curl_easy_setopt(easy, CURLOPT_HTTPHEADER, header_list);
            }
            
            curl_multi_add_handle(multi_handle_, easy);
            easy_handles.push_back(easy);
            write_data_list.push_back(std::move(write_data));
            active_transfers++;
        }
        
        // Perform transfers
        int running_handles = 0;
        auto start_time = std::chrono::steady_clock::now();
        
        do {
            CURLMcode mc = curl_multi_perform(multi_handle_, &running_handles);
            if (mc != CURLM_OK) {
                break;
            }
            
            // Check for completed transfers
            int msgs_left;
            CURLMsg* msg;
            while ((msg = curl_multi_info_read(multi_handle_, &msgs_left))) {
                if (msg->msg == CURLMSG_DONE) {
                    CURL* easy = msg->easy_handle;
                    
                    // Find corresponding chunk
                    for (size_t i = 0; i < easy_handles.size(); ++i) {
                        if (easy_handles[i] == easy) {
                            if (write_data_list[i] && write_data_list[i]->chunk) {
                                auto* chunk = write_data_list[i]->chunk;
                                if (msg->data.result == CURLE_OK) {
                                    chunk->completed = true;
                                    fclose(chunk->file_handle);
                                    chunk->file_handle = nullptr;
                                } else {
                                    chunk->attempt++;
                                    if (chunk->file_handle) {
                                        fclose(chunk->file_handle);
                                        chunk->file_handle = nullptr;
                                    }
                                }
                            }
                            break;
                        }
                    }
                    
                    curl_multi_remove_handle(multi_handle_, easy);
                    curl_easy_cleanup(easy);
                }
            }
            
            // Update progress
            if (progress_callback_) {
                auto now = std::chrono::steady_clock::now();
                auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - start_time);
                double speed = elapsed.count() > 0 ? (double)downloaded_bytes_ / elapsed.count() : 0.0;
                
                DownloadProgress progress;
                progress.total_bytes = total_size_;
                progress.downloaded_bytes = downloaded_bytes_;
                progress.speed_bytes_per_sec = speed;
                progress.active_connections = running_handles;
                
                progress_callback_(progress);
            }
            
            if (running_handles > 0) {
                curl_multi_wait(multi_handle_, nullptr, 0, 1000, nullptr);
            }
            
        } while (running_handles > 0);
        
        // Clean up remaining handles and header lists
        for (size_t i = 0; i < easy_handles.size(); ++i) {
            curl_multi_remove_handle(multi_handle_, easy_handles[i]);
            curl_easy_cleanup(easy_handles[i]);
        }
        
        // Check if all chunks completed successfully
        bool all_completed = true;
        for (const auto& chunk : chunks_) {
            if (!chunk->completed) {
                all_completed = false;
                break;
            }
        }
        
        if (all_completed) {
            return merge_chunks();
        }
        
        return false;
    }
    
    bool merge_chunks() {
        std::ofstream output(output_path_, std::ios::binary | std::ios::app);
        if (!output.is_open()) {
            return false;
        }
        
        // Sort chunks by start position
        std::sort(chunks_.begin(), chunks_.end(), 
                  [](const std::unique_ptr<DownloadChunk>& a, const std::unique_ptr<DownloadChunk>& b) {
                      return a->start < b->start;
                  });
        
        for (const auto& chunk : chunks_) {
            if (!chunk->completed) continue;
            
            std::ifstream temp_file(chunk->temp_file, std::ios::binary);
            if (temp_file.is_open()) {
                output << temp_file.rdbuf();
                temp_file.close();
                std::remove(chunk->temp_file.c_str());
            }
        }
        
        output.close();
        return true;
    }

public:
    int max_connections_;
    int max_retries_;
    size_t total_size_;
    size_t downloaded_bytes_;
    std::string url_;
    std::string output_path_;
    std::vector<std::string> headers_;
    ProgressCallback progress_callback_;
    CURLM* multi_handle_;
    std::vector<std::unique_ptr<DownloadChunk>> chunks_;
};

MultiConnectionDownloader::MultiConnectionDownloader(int max_connections, int max_retries)
    : impl_(std::make_unique<Impl>(max_connections, max_retries)) {}

MultiConnectionDownloader::~MultiConnectionDownloader() = default;

bool MultiConnectionDownloader::download(const std::string& url, const std::string& output_path,
                                        ProgressCallback progress_cb, const std::vector<std::string>& headers) {
    return impl_->download(url, output_path, progress_cb, headers);
}

void MultiConnectionDownloader::set_max_connections(int count) {
    impl_->set_max_connections(count);
}

void MultiConnectionDownloader::set_max_retries(int count) {
    impl_->set_max_retries(count);
}