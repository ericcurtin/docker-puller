#pragma once

#include <string>
#include <vector>

namespace utils {
    std::string trim(const std::string& str);
    std::vector<std::string> split(const std::string& str, char delimiter);
    std::string join(const std::vector<std::string>& parts, const std::string& separator);
    bool file_exists(const std::string& path);
    size_t get_file_size(const std::string& path);
    std::string format_bytes(size_t bytes);
    std::string get_temp_filename(const std::string& base_path);
}