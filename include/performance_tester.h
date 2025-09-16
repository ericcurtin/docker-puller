#pragma once
#include <string>
#include <vector>

struct PerformanceResult {
    int connections;
    double download_time;
    double throughput_mbps;
    bool validation_passed;
};

class PerformanceTester {
public:
    PerformanceTester(const std::string& test_model = "ai/smollm2:135M-Q4_0");
    
    std::vector<PerformanceResult> run_tests(const std::vector<int>& connection_counts);
    void report_results(const std::vector<PerformanceResult>& results);
    int find_optimal_connections(const std::vector<PerformanceResult>& results);
    
private:
    std::string test_model_;
    
    PerformanceResult test_single_configuration(int connections);
    void cleanup_test_files();
};