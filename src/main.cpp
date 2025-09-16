#include <iostream>
#include <string>
#include <chrono>
#include <iomanip>
#include <fstream>
#include <getopt.h>
#include "docker_puller.hpp"
#include "gguf_validator.hpp"

void printUsage(const char* program_name) {
    std::cout << "Usage: " << program_name << " [OPTIONS] MODEL_REFERENCE [OUTPUT_FILE]\n\n"
              << "Download Docker AI models in GGUF format.\n\n"
              << "Arguments:\n"
              << "  MODEL_REFERENCE     Docker AI model reference (e.g., ai/smollm2:135M-Q4_0)\n"
              << "  OUTPUT_FILE         Output file path (optional, defaults to model name)\n\n"
              << "Options:\n"
              << "  -c, --connections N   Number of concurrent connections (default: 1)\n"
              << "  -r, --retries N       Maximum number of retries (default: 3)\n"
              << "  -o, --output FILE     Output file path\n"
              << "  --no-resume          Disable resumable downloads\n"
              << "  --registry URL       Docker registry URL (default: https://registry-1.docker.io)\n"
              << "  --validate           Validate GGUF file after download\n"
              << "  --benchmark          Run performance benchmark with different connection counts\n"
              << "  -h, --help           Show this help message\n"
              << "  -v, --version        Show version information\n\n"
              << "Examples:\n"
              << "  " << program_name << " ai/smollm2:135M-Q4_0\n"
              << "  " << program_name << " -c 4 ai/smollm2:135M-Q4_0 model.gguf\n"
              << "  " << program_name << " --benchmark ai/smollm2:135M-Q4_0\n";
}

void printVersion() {
    std::cout << "Docker AI Model Puller v1.0.0\n"
              << "Built with libcurl multi interface\n"
              << "Supports GGUF format validation\n";
}

void printProgress(const DockerPuller::DownloadProgress& progress) {
    if (progress.total_bytes == 0) return;
    
    double percent = (double)progress.downloaded_bytes / progress.total_bytes * 100.0;
    
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - progress.start_time).count();
    
    std::cout << "\rProgress: " << std::fixed << std::setprecision(1) << percent << "% "
              << "(" << progress.downloaded_bytes << "/" << progress.total_bytes << " bytes) "
              << "Speed: " << std::setprecision(2) << progress.download_speed / 1024.0 / 1024.0 << " MB/s "
              << "Connections: " << progress.active_connections
              << " Elapsed: " << elapsed << "s" << std::flush;
}

bool runBenchmark(const std::string& model_ref, const std::string& output_file) {
    std::cout << "Running performance benchmark...\n";
    
    std::vector<int> connection_counts = {1, 2, 4, 8, 16};
    std::vector<std::pair<int, double>> results;
    
    for (int connections : connection_counts) {
        std::cout << "\nTesting with " << connections << " connections...\n";
        
        DockerPuller puller;
        DockerPuller::DownloadConfig config;
        config.model_name = model_ref;
        config.output_path = output_file + ".benchmark." + std::to_string(connections);
        config.max_connections = connections;
        config.resume_download = false;  // Start fresh for each test
        
        auto start_time = std::chrono::steady_clock::now();
        
        bool success = puller.download(config, printProgress);
        
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
        
        if (success) {
            double speed = 0.0;
            // Calculate average speed based on file size and time
            std::ifstream file(config.output_path, std::ios::binary | std::ios::ate);
            if (file.is_open()) {
                size_t file_size = file.tellg();
                speed = (double)file_size / (duration / 1000.0) / 1024.0 / 1024.0; // MB/s
                file.close();
                
                // Clean up benchmark file
                std::remove(config.output_path.c_str());
            }
            
            results.push_back({connections, speed});
            std::cout << "\nCompleted in " << duration << "ms, average speed: " 
                      << std::fixed << std::setprecision(2) << speed << " MB/s\n";
        } else {
            std::cout << "\nFailed with " << connections << " connections\n";
        }
    }
    
    // Print benchmark results
    std::cout << "\n=== Benchmark Results ===\n";
    std::cout << "Connections | Speed (MB/s)\n";
    std::cout << "------------|-------------\n";
    
    int optimal_connections = 1;
    double best_speed = 0.0;
    
    for (const auto& result : results) {
        std::cout << std::setw(11) << result.first << " | " 
                  << std::setw(11) << std::fixed << std::setprecision(2) << result.second << "\n";
        
        if (result.second > best_speed) {
            best_speed = result.second;
            optimal_connections = result.first;
        }
    }
    
    std::cout << "\nOptimal configuration: " << optimal_connections << " connections "
              << "(" << std::fixed << std::setprecision(2) << best_speed << " MB/s)\n";
    
    return !results.empty();
}

int main(int argc, char* argv[]) {
    std::string model_ref;
    std::string output_file;
    int max_connections = 1;
    int max_retries = 3;
    bool resume_download = true;
    bool validate_gguf = false;
    bool run_benchmark = false;
    std::string registry_url = "https://registry-1.docker.io";
    
    static struct option long_options[] = {
        {"connections", required_argument, 0, 'c'},
        {"retries", required_argument, 0, 'r'},
        {"output", required_argument, 0, 'o'},
        {"no-resume", no_argument, 0, 1000},
        {"registry", required_argument, 0, 1001},
        {"validate", no_argument, 0, 1002},
        {"benchmark", no_argument, 0, 1003},
        {"help", no_argument, 0, 'h'},
        {"version", no_argument, 0, 'v'},
        {0, 0, 0, 0}
    };
    
    int c;
    int option_index = 0;
    
    while ((c = getopt_long(argc, argv, "c:r:o:hv", long_options, &option_index)) != -1) {
        switch (c) {
            case 'c':
                max_connections = std::atoi(optarg);
                if (max_connections < 1 || max_connections > 32) {
                    std::cerr << "Error: Connections must be between 1 and 32\n";
                    return 1;
                }
                break;
            case 'r':
                max_retries = std::atoi(optarg);
                if (max_retries < 0 || max_retries > 10) {
                    std::cerr << "Error: Retries must be between 0 and 10\n";
                    return 1;
                }
                break;
            case 'o':
                output_file = optarg;
                break;
            case 1000: // --no-resume
                resume_download = false;
                break;
            case 1001: // --registry
                registry_url = optarg;
                break;
            case 1002: // --validate
                validate_gguf = true;
                break;
            case 1003: // --benchmark
                run_benchmark = true;
                break;
            case 'h':
                printUsage(argv[0]);
                return 0;
            case 'v':
                printVersion();
                return 0;
            case '?':
                std::cerr << "Try '" << argv[0] << " --help' for more information.\n";
                return 1;
            default:
                break;
        }
    }
    
    // Check for required model reference argument
    if (optind >= argc) {
        std::cerr << "Error: MODEL_REFERENCE is required\n";
        printUsage(argv[0]);
        return 1;
    }
    
    model_ref = argv[optind];
    
    // Set output file if not specified
    if (output_file.empty()) {
        if (optind + 1 < argc) {
            output_file = argv[optind + 1];
        } else {
            // Generate default filename from model reference
            std::string repository, tag;
            if (DockerPuller::parseModelReference(model_ref, repository, tag)) {
                size_t slash_pos = repository.find_last_of('/');
                std::string model_name = (slash_pos != std::string::npos) ? 
                                       repository.substr(slash_pos + 1) : repository;
                output_file = model_name + "_" + tag + ".gguf";
            } else {
                output_file = "model.gguf";
            }
        }
    }
    
    std::cout << "Docker AI Model Puller\n";
    std::cout << "Model: " << model_ref << "\n";
    std::cout << "Output: " << output_file << "\n";
    std::cout << "Connections: " << max_connections << "\n";
    std::cout << "Registry: " << registry_url << "\n\n";
    
    // Run benchmark if requested
    if (run_benchmark) {
        bool success = runBenchmark(model_ref, output_file);
        return success ? 0 : 1;
    }
    
    // Setup download configuration
    DockerPuller puller;
    DockerPuller::DownloadConfig config;
    config.model_name = model_ref;
    config.output_path = output_file;
    config.max_connections = max_connections;
    config.max_retries = max_retries;
    config.resume_download = resume_download;
    config.registry_url = registry_url;
    
    // Start download
    std::cout << "Starting download...\n";
    bool success = puller.download(config, printProgress);
    std::cout << "\n";
    
    if (success) {
        std::cout << "Download completed successfully!\n";
        
        // Validate GGUF file if requested
        if (validate_gguf) {
            std::cout << "Validating GGUF file...\n";
            if (GGUFValidator::validate(output_file)) {
                std::cout << "GGUF validation passed!\n";
            } else {
                std::cout << "GGUF validation failed!\n";
                return 1;
            }
        }
        
        return 0;
    } else {
        std::cout << "Download failed!\n";
        return 1;
    }
}