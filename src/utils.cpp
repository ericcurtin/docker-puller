#include "utils.h"
#include <algorithm>
#include <sstream>
#include <fstream>
#include <sys/stat.h>
#include <iomanip>
#include <random>
#include <chrono>

namespace utils {

std::string trim(const std::string& str) {
    size_t start = str.find_first_not_of(" \t\n\r");
    if (start == std::string::npos) {
        return "";
    }
    size_t end = str.find_last_not_of(" \t\n\r");
    return str.substr(start, end - start + 1);
}

std::vector<std::string> split(const std::string& str, char delimiter) {
    std::vector<std::string> tokens;
    std::stringstream ss(str);
    std::string token;
    
    while (std::getline(ss, token, delimiter)) {
        tokens.push_back(token);
    }
    
    return tokens;
}

std::string join(const std::vector<std::string>& parts, const std::string& separator) {
    if (parts.empty()) {
        return "";
    }
    
    std::ostringstream oss;
    oss << parts[0];
    
    for (size_t i = 1; i < parts.size(); ++i) {
        oss << separator << parts[i];
    }
    
    return oss.str();
}

bool file_exists(const std::string& path) {
    struct stat buffer;
    return (stat(path.c_str(), &buffer) == 0);
}

size_t get_file_size(const std::string& path) {
    struct stat stat_buf;
    if (stat(path.c_str(), &stat_buf) != 0) {
        return 0;
    }
    return stat_buf.st_size;
}

std::string format_bytes(size_t bytes) {
    const char* suffixes[] = {"B", "KB", "MB", "GB", "TB"};
    int suffix_index = 0;
    double size = static_cast<double>(bytes);
    
    while (size >= 1024.0 && suffix_index < 4) {
        size /= 1024.0;
        suffix_index++;
    }
    
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1) << size << " " << suffixes[suffix_index];
    return oss.str();
}

std::string get_temp_filename(const std::string& base_path) {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(1000, 9999);
    
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    
    return base_path + ".tmp." + std::to_string(timestamp) + "." + std::to_string(dis(gen));
}

} // namespace utils