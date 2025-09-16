#include "docker_puller.h"
#include "url_parser.h"
#include <iostream>
#include <string>
#include <chrono>
#include <iomanip>

void print_usage(const char* program_name) {
    std::cout << "Usage: " << program_name << " [OPTIONS] MODEL_SPEC [OUTPUT_FILE]\n"
              << "\n"
              << "Pull Docker AI models with concurrent connections and resume support.\n"
              << "\n"
              << "Arguments:\n"
              << "  MODEL_SPEC    Docker AI model specification (e.g., ai/smollm2:135M-Q4_0)\n"
              << "  OUTPUT_FILE   Output file path (optional, defaults to MODEL_NAME.gguf)\n"
              << "\n"
              << "Options:\n"
              << "  -c, --connections NUM  Number of concurrent connections (default: 1, max: 16)\n"
              << "  -r, --retries NUM      Number of retry attempts (default: 3)\n"
              << "  -h, --help             Show this help message\n"
              << "  -v, --version          Show version information\n"
              << "\n"
              << "Examples:\n"
              << "  " << program_name << " ai/smollm2:135M-Q4_0\n"
              << "  " << program_name << " -c 4 ai/smollm2:135M-Q4_0 smollm2.gguf\n"
              << "  " << program_name << " --connections 8 --retries 5 ai/llama3:8B-Q4_0\n"
              << std::endl;
}

void print_version() {
    std::cout << "docker-puller 1.0.0\n"
              << "Docker AI model puller with concurrent downloads and resume support\n"
              << "Built with libcurl multi interface\n"
              << std::endl;
}

std::string generate_output_filename(const std::string& model_spec) {
    ModelSpec spec = UrlParser::parse_model_spec(model_spec);
    if (!spec.is_valid()) {
        return "model.gguf";
    }
    return spec.model_name + "_" + spec.tag + ".gguf";
}

void progress_callback(const DownloadProgress& progress) {
    static auto start_time = std::chrono::steady_clock::now();
    auto current_time = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(current_time - start_time).count();
    
    if (progress.total_size > 0) {
        double percentage = (static_cast<double>(progress.downloaded_bytes) / progress.total_size) * 100.0;
        double speed_mbps = elapsed > 0 ? (static_cast<double>(progress.downloaded_bytes) / (1024 * 1024)) / elapsed : 0.0;
        
        std::cout << "\r" << std::fixed << std::setprecision(1)
                  << "Progress: " << percentage << "% "
                  << "(" << progress.downloaded_bytes / (1024 * 1024) << "/" 
                  << progress.total_size / (1024 * 1024) << " MB) "
                  << "Speed: " << speed_mbps << " MB/s "
                  << "Connections: " << progress.active_connections
                  << std::flush;
    } else {
        std::cout << "\r" << "Downloaded: " << progress.downloaded_bytes / (1024 * 1024) << " MB"
                  << std::flush;
    }
}

int main(int argc, char* argv[]) {
    int connections = 1;
    int retries = 3;
    std::string model_spec;
    std::string output_file;
    
    // Parse command line arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "-h" || arg == "--help") {
            print_usage(argv[0]);
            return 0;
        } else if (arg == "-v" || arg == "--version") {
            print_version();
            return 0;
        } else if (arg == "-c" || arg == "--connections") {
            if (i + 1 >= argc) {
                std::cerr << "Error: " << arg << " requires a number" << std::endl;
                return 1;
            }
            try {
                connections = std::stoi(argv[++i]);
                if (connections < 1 || connections > 16) {
                    std::cerr << "Error: Number of connections must be between 1 and 16" << std::endl;
                    return 1;
                }
            } catch (const std::exception&) {
                std::cerr << "Error: Invalid number for connections: " << argv[i] << std::endl;
                return 1;
            }
        } else if (arg == "-r" || arg == "--retries") {
            if (i + 1 >= argc) {
                std::cerr << "Error: " << arg << " requires a number" << std::endl;
                return 1;
            }
            try {
                retries = std::stoi(argv[++i]);
                if (retries < 1 || retries > 10) {
                    std::cerr << "Error: Number of retries must be between 1 and 10" << std::endl;
                    return 1;
                }
            } catch (const std::exception&) {
                std::cerr << "Error: Invalid number for retries: " << argv[i] << std::endl;
                return 1;
            }
        } else if (arg.substr(0, 1) == "-") {
            std::cerr << "Error: Unknown option: " << arg << std::endl;
            print_usage(argv[0]);
            return 1;
        } else if (model_spec.empty()) {
            model_spec = arg;
        } else if (output_file.empty()) {
            output_file = arg;
        } else {
            std::cerr << "Error: Too many arguments" << std::endl;
            print_usage(argv[0]);
            return 1;
        }
    }
    
    // Validate required arguments
    if (model_spec.empty()) {
        std::cerr << "Error: MODEL_SPEC is required" << std::endl;
        print_usage(argv[0]);
        return 1;
    }
    
    if (!UrlParser::is_valid_model_spec(model_spec)) {
        std::cerr << "Error: Invalid model specification: " << model_spec << std::endl;
        std::cerr << "Expected format: namespace/model:tag (e.g., ai/smollm2:135M-Q4_0)" << std::endl;
        return 1;
    }
    
    if (output_file.empty()) {
        output_file = generate_output_filename(model_spec);
    }
    
    // Print configuration
    std::cout << "Docker AI Model Puller v1.0.0" << std::endl;
    std::cout << "Model: " << model_spec << std::endl;
    std::cout << "Output: " << output_file << std::endl;
    std::cout << "Connections: " << connections << std::endl;
    std::cout << "Max retries: " << retries << std::endl;
    std::cout << std::endl;
    
    // Create puller and start download
    DockerPuller puller(connections, retries);
    puller.set_progress_callback(progress_callback);
    
    auto start_time = std::chrono::steady_clock::now();
    bool success = puller.pull(model_spec, output_file);
    auto end_time = std::chrono::steady_clock::now();
    
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(end_time - start_time);
    
    std::cout << std::endl;
    
    if (success) {
        std::cout << "✓ Download completed successfully in " << duration.count() << " seconds" << std::endl;
        std::cout << "✓ Model saved to: " << output_file << std::endl;
        return 0;
    } else {
        std::cout << "✗ Download failed after " << duration.count() << " seconds" << std::endl;
        return 1;
    }
}