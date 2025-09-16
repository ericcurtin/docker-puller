#include <iostream>
#include <string>
#include <vector>
#include <cstring>
#include "../include/docker_ai_puller.h"
#include "../include/performance_tester.h"

void print_usage(const char* program_name) {
    std::cout << "Usage: " << program_name << " [OPTIONS] MODEL_SPEC\n"
              << "\nPull Docker AI models in GGUF format with multi-connection acceleration\n"
              << "\nOPTIONS:\n"
              << "  -c, --connections NUM    Number of concurrent connections (default: 1, max: 16)\n"
              << "  -r, --retries NUM        Number of retry attempts (default: 3)\n"
              << "  -o, --output PATH        Output file path (default: auto-generated)\n"
              << "  -t, --test              Run performance tests\n"
              << "  -h, --help              Show this help message\n"
              << "\nEXAMPLES:\n"
              << "  " << program_name << " ai/smollm2:135M-Q4_0\n"
              << "  " << program_name << " -c 4 ai/smollm2:135M-Q4_0\n"
              << "  " << program_name << " -c 8 -o my_model.gguf ai/smollm2:135M-Q4_0\n"
              << "  " << program_name << " --test\n"
              << std::endl;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }
    
    int connections = 1;
    int retries = 3;
    std::string output_path;
    std::string model_spec;
    bool run_tests = false;
    
    // Parse command line arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "-h" || arg == "--help") {
            print_usage(argv[0]);
            return 0;
        } else if (arg == "-t" || arg == "--test") {
            run_tests = true;
        } else if ((arg == "-c" || arg == "--connections") && i + 1 < argc) {
            connections = std::atoi(argv[++i]);
            if (connections < 1 || connections > 16) {
                std::cerr << "Error: Connections must be between 1 and 16" << std::endl;
                return 1;
            }
        } else if ((arg == "-r" || arg == "--retries") && i + 1 < argc) {
            retries = std::atoi(argv[++i]);
            if (retries < 0 || retries > 10) {
                std::cerr << "Error: Retries must be between 0 and 10" << std::endl;
                return 1;
            }
        } else if ((arg == "-o" || arg == "--output") && i + 1 < argc) {
            output_path = argv[++i];
        } else if (arg[0] != '-') {
            model_spec = arg;
        } else {
            std::cerr << "Error: Unknown option " << arg << std::endl;
            print_usage(argv[0]);
            return 1;
        }
    }
    
    // Initialize curl globally
    if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK) {
        std::cerr << "Error: Failed to initialize libcurl" << std::endl;
        return 1;
    }
    
    int result = 0;
    
    try {
        if (run_tests) {
            std::cout << "Running performance tests..." << std::endl;
            PerformanceTester tester;
            std::vector<int> test_connections = {1, 2, 4, 8, 16};
            auto results = tester.run_tests(test_connections);
            tester.report_results(results);
            
            int optimal = tester.find_optimal_connections(results);
            std::cout << "\nRecommended optimal connection count: " << optimal << std::endl;
        } else {
            if (model_spec.empty()) {
                std::cerr << "Error: MODEL_SPEC is required" << std::endl;
                print_usage(argv[0]);
                result = 1;
            } else {
                DockerAIPuller puller(connections, retries);
                
                std::cout << "Pulling model: " << model_spec << std::endl;
                std::cout << "Using " << connections << " connection(s)" << std::endl;
                std::cout << "Max retries: " << retries << std::endl;
                
                if (puller.pull_model(model_spec, output_path)) {
                    std::cout << "Model downloaded successfully!" << std::endl;
                } else {
                    std::cerr << "Error: Failed to download model" << std::endl;
                    result = 1;
                }
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        result = 1;
    }
    
    curl_global_cleanup();
    return result;
}