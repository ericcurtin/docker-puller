#include <iostream>
#include <fstream>
#include <curl/curl.h>
#include "../include/docker_registry.h"

struct DownloadData {
    std::ofstream file;
    size_t total_downloaded;
    
    DownloadData(const std::string& filename) : file(filename, std::ios::binary), total_downloaded(0) {}
};

size_t write_callback(void* contents, size_t size, size_t nmemb, DownloadData* data) {
    size_t total_size = size * nmemb;
    if (data->file.is_open()) {
        data->file.write((char*)contents, total_size);
        if (data->file.good()) {
            data->total_downloaded += total_size;
            return total_size;
        }
    }
    return 0;
}

int main() {
    try {
        DockerAIModel model = DockerAIModel::parse("ai/smollm2:135M-Q4_0");
        DockerRegistry registry;
        
        std::cout << "Getting auth token...\n";
        std::string auth_token = registry.get_auth_token(model);
        
        std::cout << "Getting manifest layers...\n";
        auto layers = registry.get_manifest_layers(model);
        
        ManifestLayer* gguf_layer = nullptr;
        for (auto& layer : layers) {
            if (layer.media_type == "application/vnd.docker.ai.gguf.v3") {
                gguf_layer = &layer;
                break;
            }
        }
        
        if (!gguf_layer) {
            std::cerr << "No GGUF layer found\n";
            return 1;
        }
        
        std::string download_url = registry.get_download_url(model, gguf_layer->digest);
        std::cout << "Download URL: " << download_url << "\n";
        
        // Test just getting the headers (file size check)
        curl_global_init(CURL_GLOBAL_DEFAULT);
        CURL* curl = curl_easy_init();
        
        if (!curl) {
            std::cerr << "Failed to init curl\n";
            return 1;
        }
        
        struct curl_slist* headers = nullptr;
        headers = curl_slist_append(headers, ("Authorization: Bearer " + auth_token).c_str());
        
        curl_easy_setopt(curl, CURLOPT_URL, download_url.c_str());
        curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        
        CURLcode res = curl_easy_perform(curl);
        
        if (res == CURLE_OK) {
            long response_code;
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
            
            curl_off_t content_length;
            curl_easy_getinfo(curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD_T, &content_length);
            
            std::cout << "Response code: " << response_code << "\n";
            std::cout << "Content length: " << content_length << " bytes\n";
            
            if (response_code == 200 && content_length > 0) {
                std::cout << "✓ Head request successful!\n";
                
                // Download a small chunk to test the download mechanism
                curl_easy_setopt(curl, CURLOPT_NOBODY, 0L);
                curl_easy_setopt(curl, CURLOPT_RANGE, "0-1023");  // First 1KB only
                
                DownloadData download_data("test_chunk.bin");
                curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
                curl_easy_setopt(curl, CURLOPT_WRITEDATA, &download_data);
                
                res = curl_easy_perform(curl);
                
                if (res == CURLE_OK) {
                    std::cout << "✓ Downloaded " << download_data.total_downloaded << " bytes\n";
                } else {
                    std::cout << "✗ Download failed: " << curl_easy_strerror(res) << "\n";
                }
            } else {
                std::cout << "✗ Head request failed\n";
            }
        } else {
            std::cout << "✗ CURL error: " << curl_easy_strerror(res) << "\n";
        }
        
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        curl_global_cleanup();
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}