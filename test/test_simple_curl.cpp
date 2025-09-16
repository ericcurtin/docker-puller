#include <iostream>
#include <curl/curl.h>
#include <fstream>

struct WriteData {
    std::ofstream* file;
};

size_t write_callback(void* contents, size_t size, size_t nmemb, WriteData* data) {
    size_t total_size = size * nmemb;
    if (data->file && data->file->is_open()) {
        data->file->write((char*)contents, total_size);
        return data->file->good() ? total_size : 0;
    }
    return 0;
}

int main() {
    curl_global_init(CURL_GLOBAL_DEFAULT);
    
    CURL* curl = curl_easy_init();
    if (!curl) {
        std::cerr << "Failed to init curl\n";
        return 1;
    }
    
    // Simple HTTP test first
    std::string test_url = "https://httpbin.org/status/200";
    curl_easy_setopt(curl, CURLOPT_URL, test_url.c_str());
    curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
    
    CURLcode res = curl_easy_perform(curl);
    
    if (res != CURLE_OK) {
        std::cerr << "HTTP test failed: " << curl_easy_strerror(res) << "\n";
        curl_easy_cleanup(curl);
        curl_global_cleanup();
        return 1;
    }
    
    long response_code;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
    std::cout << "HTTP test successful, response code: " << response_code << "\n";
    
    curl_easy_cleanup(curl);
    curl_global_cleanup();
    return 0;
}