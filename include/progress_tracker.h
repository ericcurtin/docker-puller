#pragma once

#include <chrono>
#include <string>

class ProgressTracker {
public:
    ProgressTracker(size_t total_size);
    
    void update(size_t bytes_downloaded);
    void print_progress();
    double get_download_speed() const;
    std::string get_eta() const;
    
private:
    size_t total_size_;
    size_t bytes_downloaded_;
    std::chrono::steady_clock::time_point start_time_;
    std::chrono::steady_clock::time_point last_update_;
    
    std::string format_bytes(size_t bytes) const;
    std::string format_time(double seconds) const;
};