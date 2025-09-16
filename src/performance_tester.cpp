#include "../include/performance_tester.h"
#include "../include/docker_ai_puller.h"
#include <iostream>
#include <chrono>
#include <iomanip>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <thread>

PerformanceTester::PerformanceTester(const std::string& test_model) 
    : test_model_(test_model) {
}

std::vector<PerformanceResult> PerformanceTester::run_tests(const std::vector<int>& connection_counts) {
    std::vector<PerformanceResult> results;
    
    std::cout << "Running performance tests with model: " << test_model_ << std::endl;
    std::cout << "Testing connection counts: ";
    for (int count : connection_counts) {
        std::cout << count << " ";
    }
    std::cout << std::endl << std::endl;
    
    for (int connections : connection_counts) {
        std::cout << "Testing with " << connections << " connection(s)..." << std::endl;
        
        PerformanceResult result = test_single_configuration(connections);
        results.push_back(result);
        
        std::cout << "  Time: " << std::fixed << std::setprecision(2) << result.download_time << "s"
                  << ", Throughput: " << result.throughput_mbps << " MB/s"
                  << ", Validation: " << (result.validation_passed ? "PASS" : "FAIL") << std::endl;
        
        // Clean up between tests
        cleanup_test_files();
        
        // Small delay between tests
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    
    return results;
}

PerformanceResult PerformanceTester::test_single_configuration(int connections) {
    PerformanceResult result;
    result.connections = connections;
    result.validation_passed = false;
    result.download_time = 0.0;
    result.throughput_mbps = 0.0;
    
    // Create a unique test filename
    std::string test_filename = "test_model_" + std::to_string(connections) + "_conn.gguf";
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    try {
        DockerAIPuller puller(connections, 1); // Single retry for testing
        
        if (puller.pull_model(test_model_, test_filename)) {
            auto end_time = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
            
            result.download_time = duration.count() / 1000.0;
            
            // Calculate throughput
            if (std::filesystem::exists(test_filename)) {
                auto file_size = std::filesystem::file_size(test_filename);
                double file_size_mb = file_size / (1024.0 * 1024.0);
                result.throughput_mbps = file_size_mb / result.download_time;
                
                // Validate the downloaded file
                result.validation_passed = puller.validate_gguf(test_filename);
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "  Error during test: " << e.what() << std::endl;
    }
    
    return result;
}

void PerformanceTester::cleanup_test_files() {
    // Remove test files
    try {
        for (const auto& entry : std::filesystem::directory_iterator(".")) {
            if (entry.is_regular_file()) {
                std::string filename = entry.path().filename().string();
                if (filename.find("test_model_") == 0 && filename.find("_conn.gguf") != std::string::npos) {
                    std::filesystem::remove(entry.path());
                }
            }
        }
    } catch (const std::exception& e) {
        // Ignore cleanup errors
    }
}

void PerformanceTester::report_results(const std::vector<PerformanceResult>& results) {
    std::cout << "\n=== Performance Test Results ===" << std::endl;
    std::cout << std::setw(12) << "Connections" 
              << std::setw(12) << "Time (s)" 
              << std::setw(15) << "Throughput (MB/s)" 
              << std::setw(12) << "Validation" << std::endl;
    std::cout << std::string(51, '-') << std::endl;
    
    for (const auto& result : results) {
        std::cout << std::setw(12) << result.connections
                  << std::setw(12) << std::fixed << std::setprecision(2) << result.download_time
                  << std::setw(15) << std::fixed << std::setprecision(2) << result.throughput_mbps
                  << std::setw(12) << (result.validation_passed ? "PASS" : "FAIL") << std::endl;
    }
    
    // Find best performance
    auto best_result = std::max_element(results.begin(), results.end(),
        [](const PerformanceResult& a, const PerformanceResult& b) {
            if (!a.validation_passed && b.validation_passed) return true;
            if (a.validation_passed && !b.validation_passed) return false;
            return a.throughput_mbps < b.throughput_mbps;
        });
    
    if (best_result != results.end() && best_result->validation_passed) {
        std::cout << "\nBest performance: " << best_result->connections 
                  << " connections (" << best_result->throughput_mbps << " MB/s)" << std::endl;
    }
}

int PerformanceTester::find_optimal_connections(const std::vector<PerformanceResult>& results) {
    // Find the optimal number of connections based on throughput
    int optimal = 1;
    double best_throughput = 0.0;
    
    for (const auto& result : results) {
        if (result.validation_passed && result.throughput_mbps > best_throughput) {
            best_throughput = result.throughput_mbps;
            optimal = result.connections;
        }
    }
    
    // Check for diminishing returns - if improvement is less than 10%, prefer fewer connections
    for (const auto& result : results) {
        if (result.validation_passed && result.connections < optimal) {
            double improvement = (best_throughput - result.throughput_mbps) / result.throughput_mbps;
            if (improvement < 0.1) { // Less than 10% improvement
                optimal = result.connections;
                break;
            }
        }
    }
    
    return optimal;
}