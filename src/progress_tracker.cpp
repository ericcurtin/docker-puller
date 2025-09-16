#include "progress_tracker.h"
#include <iostream>
#include <iomanip>
#include <sstream>

ProgressTracker::ProgressTracker(size_t total_size)
    : total_size_(total_size), bytes_downloaded_(0) {
    start_time_ = std::chrono::steady_clock::now();
    last_update_ = start_time_;
}

void ProgressTracker::update(size_t bytes_downloaded) {
    bytes_downloaded_ = bytes_downloaded;
    last_update_ = std::chrono::steady_clock::now();
}

void ProgressTracker::print_progress() {
    if (total_size_ == 0) return;
    
    double percentage = (double)bytes_downloaded_ / total_size_ * 100.0;
    double speed = get_download_speed();
    std::string eta = get_eta();
    
    // Create progress bar
    const int bar_width = 40;
    int filled = (int)(percentage / 100.0 * bar_width);
    
    std::cout << "\r[";
    for (int i = 0; i < bar_width; ++i) {
        if (i < filled) {
            std::cout << "=";
        } else if (i == filled) {
            std::cout << ">";
        } else {
            std::cout << " ";
        }
    }
    std::cout << "] " << std::fixed << std::setprecision(1) << percentage << "% "
              << format_bytes(bytes_downloaded_) << "/" << format_bytes(total_size_)
              << " @ " << format_bytes((size_t)speed) << "/s"
              << " ETA: " << eta;
    std::cout.flush();
}

double ProgressTracker::get_download_speed() const {
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(
        last_update_ - start_time_).count();
    
    if (duration == 0) return 0.0;
    
    return (double)bytes_downloaded_ / duration;
}

std::string ProgressTracker::get_eta() const {
    double speed = get_download_speed();
    if (speed <= 0 || bytes_downloaded_ >= total_size_) {
        return "00:00";
    }
    
    size_t remaining_bytes = total_size_ - bytes_downloaded_;
    double eta_seconds = remaining_bytes / speed;
    
    return format_time(eta_seconds);
}

std::string ProgressTracker::format_bytes(size_t bytes) const {
    const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    int unit_index = 0;
    double size = (double)bytes;
    
    while (size >= 1024.0 && unit_index < 4) {
        size /= 1024.0;
        unit_index++;
    }
    
    std::ostringstream oss;
    if (unit_index == 0) {
        oss << (int)size << " " << units[unit_index];
    } else {
        oss << std::fixed << std::setprecision(1) << size << " " << units[unit_index];
    }
    
    return oss.str();
}

std::string ProgressTracker::format_time(double seconds) const {
    int hours = (int)seconds / 3600;
    int minutes = ((int)seconds % 3600) / 60;
    int secs = (int)seconds % 60;
    
    std::ostringstream oss;
    if (hours > 0) {
        oss << std::setfill('0') << std::setw(2) << hours << ":"
            << std::setfill('0') << std::setw(2) << minutes << ":"
            << std::setfill('0') << std::setw(2) << secs;
    } else {
        oss << std::setfill('0') << std::setw(2) << minutes << ":"
            << std::setfill('0') << std::setw(2) << secs;
    }
    
    return oss.str();
}