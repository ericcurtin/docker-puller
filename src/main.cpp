#include "docker_ai_puller.h"
#include <iostream>
#include <string>
#include <chrono>
#include <iomanip>

void print_usage(const char* program_name) {
    std::cout << "Usage: " << program_name << " [OPTIONS] MODEL_REF\n"
              << "\n"
              << "Download Docker AI models (GGUF format) with concurrent connections\n"
              << "\n"
              << "Options:\n"
              << "  -c, --connections NUM    Number of concurrent connections (default: 1)\n"
              << "  -o, --output DIR         Output directory (default: current directory)\n"
              << "  -t, --token TOKEN        Authentication token for registry\n"
              << "  -h, --help               Show this help message\n"
              << "\n"
              << "Examples:\n"
              << "  " << program_name << " ai/smollm2:135M-Q4_0\n"
              << "  " << program_name << " -c 4 ai/smollm2:135M-Q4_0\n"
              << "  " << program_name << " -c 8 -o ./models ai/smollm2:135M-Q4_0\n"
              << std::endl;
}

int main(int argc, char* argv[]) {
    std::string model_ref;
    std::string output_dir = ".";
    std::string auth_token;
    int connections = 1;
    
    // Parse command line arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "-h" || arg == "--help") {
            print_usage(argv[0]);
            return 0;
        } else if (arg == "-c" || arg == "--connections") {
            if (i + 1 < argc) {
                connections = std::stoi(argv[++i]);
                if (connections < 1 || connections > 32) {
                    std::cerr << "Error: Number of connections must be between 1 and 32" << std::endl;
                    return 1;
                }
            } else {
                std::cerr << "Error: " << arg << " requires a value" << std::endl;
                return 1;
            }
        } else if (arg == "-o" || arg == "--output") {
            if (i + 1 < argc) {
                output_dir = argv[++i];
            } else {
                std::cerr << "Error: " << arg << " requires a value" << std::endl;
                return 1;
            }
        } else if (arg == "-t" || arg == "--token") {
            if (i + 1 < argc) {
                auth_token = argv[++i];
            } else {
                std::cerr << "Error: " << arg << " requires a value" << std::endl;
                return 1;
            }
        } else if (arg.substr(0, 1) == "-") {
            std::cerr << "Error: Unknown option " << arg << std::endl;
            print_usage(argv[0]);
            return 1;
        } else {
            if (model_ref.empty()) {
                model_ref = arg;
            } else {
                std::cerr << "Error: Multiple model references provided" << std::endl;
                return 1;
            }
        }
    }
    
    if (model_ref.empty()) {
        std::cerr << "Error: Model reference is required" << std::endl;
        print_usage(argv[0]);
        return 1;
    }
    
    std::cout << "Docker AI Model Puller v1.0" << std::endl;
    std::cout << "=============================" << std::endl;
    std::cout << "Model: " << model_ref << std::endl;
    std::cout << "Connections: " << connections << std::endl;
    std::cout << "Output directory: " << output_dir << std::endl;
    std::cout << std::endl;
    
    try {
        // Create the puller
        DockerAIPuller puller(connections);
        
        if (!auth_token.empty()) {
            puller.set_auth_token(auth_token);
        }
        
        // Set up progress callback
        auto last_progress_time = std::chrono::steady_clock::now();
        puller.set_progress_callback([&last_progress_time](size_t downloaded, size_t total) {
            auto now = std::chrono::steady_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_progress_time);
            
            // Update progress every 1 second
            if (duration.count() >= 1000) {
                double percentage = (double)downloaded / total * 100.0;
                double mb_downloaded = (double)downloaded / (1024 * 1024);
                double mb_total = (double)total / (1024 * 1024);
                
                std::cout << "\rProgress: " << std::fixed << std::setprecision(1) 
                          << percentage << "% (" << mb_downloaded << "/" << mb_total << " MB)" << std::flush;
                last_progress_time = now;
            }
        });
        
        // Start the download
        auto start_time = std::chrono::high_resolution_clock::now();
        
        bool success = puller.pull_model(model_ref, output_dir);
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::seconds>(end_time - start_time);
        
        std::cout << std::endl; // New line after progress
        
        if (success) {
            std::cout << "Download completed successfully in " << duration.count() << " seconds" << std::endl;
            return 0;
        } else {
            std::cerr << "Download failed" << std::endl;
            return 1;
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}