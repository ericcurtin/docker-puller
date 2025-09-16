#include <iostream>
#include <string>
#include <chrono>
#include <iomanip>
#include <getopt.h>

#include "docker_registry.h"
#include "downloader.h"
#include "gguf_validator.h"
#include "utils.h"

void print_usage(const char* program_name) {
    std::cout << "Usage: " << program_name << " [OPTIONS] MODEL_SPEC\n"
              << "\n"
              << "Download Docker AI models in GGUF format\n"
              << "\n"
              << "Arguments:\n"
              << "  MODEL_SPEC    Model specification (e.g., ai/smollm2:135M-Q4_0)\n"
              << "\n"
              << "Options:\n"
              << "  -c, --connections NUM  Number of concurrent connections (default: 1)\n"
              << "  -o, --output PATH      Output file path (default: model filename)\n"
              << "  -r, --retries NUM      Number of retry attempts (default: 3)\n"
              << "  -v, --verbose          Verbose output\n"
              << "  -h, --help             Show this help message\n";
}

void print_progress(const DownloadProgress& progress) {
    static auto last_update = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();
    
    // Update progress every 500ms
    if (std::chrono::duration_cast<std::chrono::milliseconds>(now - last_update).count() < 500) {
        return;
    }
    last_update = now;
    
    double percent = progress.total_bytes > 0 ? 
        (double)progress.downloaded_bytes / progress.total_bytes * 100.0 : 0.0;
    
    std::cout << "\rProgress: " << std::fixed << std::setprecision(1) << percent << "% "
              << "(" << utils::format_bytes(progress.downloaded_bytes) 
              << "/" << utils::format_bytes(progress.total_bytes) << ") "
              << "Speed: " << utils::format_bytes((size_t)progress.speed_bytes_per_sec) << "/s "
              << "Connections: " << progress.active_connections
              << std::flush;
}

int main(int argc, char* argv[]) {
    int connections = 1;
    int retries = 3;
    std::string output_path;
    bool verbose = false;
    
    static struct option long_options[] = {
        {"connections", required_argument, 0, 'c'},
        {"output", required_argument, 0, 'o'},
        {"retries", required_argument, 0, 'r'},
        {"verbose", no_argument, 0, 'v'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };
    
    int opt;
    int option_index = 0;
    
    while ((opt = getopt_long(argc, argv, "c:o:r:vh", long_options, &option_index)) != -1) {
        switch (opt) {
            case 'c':
                connections = std::atoi(optarg);
                if (connections < 1 || connections > 16) {
                    std::cerr << "Error: connections must be between 1 and 16\n";
                    return 1;
                }
                break;
            case 'o':
                output_path = optarg;
                break;
            case 'r':
                retries = std::atoi(optarg);
                if (retries < 0 || retries > 10) {
                    std::cerr << "Error: retries must be between 0 and 10\n";
                    return 1;
                }
                break;
            case 'v':
                verbose = true;
                break;
            case 'h':
                print_usage(argv[0]);
                return 0;
            default:
                print_usage(argv[0]);
                return 1;
        }
    }
    
    if (optind >= argc) {
        std::cerr << "Error: MODEL_SPEC is required\n";
        print_usage(argv[0]);
        return 1;
    }
    
    std::string model_spec = argv[optind];
    
    try {
        if (verbose) {
            std::cout << "Parsing model specification: " << model_spec << "\n";
        }
        
        DockerAIModel model = DockerAIModel::parse(model_spec);
        
        if (verbose) {
            std::cout << "Parsed model: " << model.to_string() << "\n";
            std::cout << "Using " << connections << " concurrent connections\n";
            std::cout << "Maximum retries: " << retries << "\n";
        }
        
        DockerRegistry registry;
        std::cout << "Fetching manifest for " << model.to_string() << "...\n";
        
        auto layers = registry.get_manifest_layers(model);
        
        if (layers.empty()) {
            std::cerr << "Error: No GGUF layers found in manifest\n";
            return 1;
        }
        
        // Find the GGUF layer
        ManifestLayer* gguf_layer = nullptr;
        for (auto& layer : layers) {
            if (layer.media_type == "application/vnd.docker.ai.gguf.v3") {
                gguf_layer = &layer;
                break;
            }
        }
        
        if (!gguf_layer) {
            std::cerr << "Error: No GGUF layer found with media type 'application/vnd.docker.ai.gguf.v3'\n";
            return 1;
        }
        
        if (output_path.empty()) {
            output_path = model.repository + "_" + model.tag + ".gguf";
        }
        
        if (verbose) {
            std::cout << "GGUF layer size: " << utils::format_bytes(gguf_layer->size) << "\n";
            std::cout << "Output file: " << output_path << "\n";
        }
        
        std::string download_url = registry.get_download_url(model, gguf_layer->digest);
        std::string auth_token = registry.get_auth_token(model);
        
        std::vector<std::string> download_headers = {
            "Authorization: Bearer " + auth_token
        };
        
        MultiConnectionDownloader downloader(connections, retries);
        
        std::cout << "Downloading GGUF model...\n";
        auto start_time = std::chrono::steady_clock::now();
        
        bool success = downloader.download(download_url, output_path, 
                                          verbose ? print_progress : nullptr, download_headers);
        
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::seconds>(end_time - start_time);
        
        std::cout << "\n";
        
        if (!success) {
            std::cerr << "Error: Download failed after " << retries << " retries\n";
            return 1;
        }
        
        std::cout << "Download completed in " << duration.count() << " seconds\n";
        
        // Validate GGUF format
        std::cout << "Validating GGUF format...\n";
        if (!GgufValidator::validate_file(output_path)) {
            std::cerr << "Error: Downloaded file is not a valid GGUF file\n";
            return 1;
        }
        
        std::cout << "GGUF validation successful!\n";
        std::cout << "Model saved to: " << output_path << "\n";
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
    
    return 0;
}