#include <iostream>
#include <string>
#include <algorithm>
#include <getopt.h>
#include "docker_puller.h"
#include "url_resolver.h"

void print_usage(const char* program_name) {
    std::cout << "Usage: " << program_name << " [OPTIONS] MODEL_SPEC [OUTPUT_FILE]\n"
              << "\nPull Docker AI models with concurrent downloads\n"
              << "\nArguments:\n"
              << "  MODEL_SPEC    Docker AI model specification (e.g., ai/smollm2:135M-Q4_0)\n"
              << "  OUTPUT_FILE   Output file path (optional, defaults to model name)\n"
              << "\nOptions:\n"
              << "  -c, --connections NUM   Number of concurrent connections (default: 1)\n"
              << "  -r, --retries NUM       Number of retries on failure (default: 3)\n"
              << "  -v, --verbose           Enable verbose output\n"
              << "  -h, --help              Show this help message\n"
              << "  --no-resume             Disable resumable downloads\n"
              << "\nExamples:\n"
              << "  " << program_name << " ai/smollm2:135M-Q4_0\n"
              << "  " << program_name << " -c 4 ai/smollm2:135M-Q4_0 model.gguf\n"
              << std::endl;
}

int main(int argc, char* argv[]) {
    DownloadConfig config;
    config.max_connections = 1;
    config.max_retries = 3;
    config.resume = true;
    config.verbose = false;
    
    static struct option long_options[] = {
        {"connections", required_argument, 0, 'c'},
        {"retries", required_argument, 0, 'r'},
        {"verbose", no_argument, 0, 'v'},
        {"help", no_argument, 0, 'h'},
        {"no-resume", no_argument, 0, 1},
        {0, 0, 0, 0}
    };
    
    int opt;
    int option_index = 0;
    
    while ((opt = getopt_long(argc, argv, "c:r:vh", long_options, &option_index)) != -1) {
        switch (opt) {
            case 'c':
                config.max_connections = std::stoi(optarg);
                if (config.max_connections < 1 || config.max_connections > 16) {
                    std::cerr << "Error: Number of connections must be between 1 and 16\n";
                    return 1;
                }
                break;
            case 'r':
                config.max_retries = std::stoi(optarg);
                if (config.max_retries < 0 || config.max_retries > 10) {
                    std::cerr << "Error: Number of retries must be between 0 and 10\n";
                    return 1;
                }
                break;
            case 'v':
                config.verbose = true;
                break;
            case 'h':
                print_usage(argv[0]);
                return 0;
            case 1: // --no-resume
                config.resume = false;
                break;
            default:
                print_usage(argv[0]);
                return 1;
        }
    }
    
    if (optind >= argc) {
        std::cerr << "Error: MODEL_SPEC is required\n\n";
        print_usage(argv[0]);
        return 1;
    }
    
    std::string model_spec = argv[optind];
    
    // Validate model specification
    if (!UrlResolver::is_valid_docker_ai_model(model_spec)) {
        std::cerr << "Error: Invalid Docker AI model specification: " << model_spec << std::endl;
        return 1;
    }
    
    // Resolve Docker AI model URL
    config.url = UrlResolver::resolve_docker_ai_model_url(model_spec);
    if (config.url.empty()) {
        std::cerr << "Error: Failed to resolve URL for model: " << model_spec << std::endl;
        return 1;
    }
    
    // Set output file
    if (optind + 1 < argc) {
        config.output_file = argv[optind + 1];
    } else {
        // Generate default filename from model spec
        std::string filename = model_spec;
        std::replace(filename.begin(), filename.end(), '/', '_');
        std::replace(filename.begin(), filename.end(), ':', '_');
        config.output_file = filename + ".gguf";
    }
    
    if (config.verbose) {
        std::cout << "Configuration:\n"
                  << "  Model: " << model_spec << "\n"
                  << "  URL: " << config.url << "\n"
                  << "  Output: " << config.output_file << "\n"
                  << "  Connections: " << config.max_connections << "\n"
                  << "  Retries: " << config.max_retries << "\n"
                  << "  Resume: " << (config.resume ? "yes" : "no") << "\n"
                  << std::endl;
    }
    
    // Initialize curl globally
    if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK) {
        std::cerr << "Error: Failed to initialize curl" << std::endl;
        return 1;
    }
    
    // Create puller and start download
    DockerPuller puller(config);
    bool success = puller.download();
    
    // Cleanup curl
    curl_global_cleanup();
    
    if (success) {
        std::cout << "Download completed successfully: " << config.output_file << std::endl;
        return 0;
    } else {
        std::cerr << "Download failed" << std::endl;
        return 1;
    }
}