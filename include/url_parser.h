#ifndef URL_PARSER_H
#define URL_PARSER_H

#include <string>

struct ModelSpec {
    std::string namespace_name;  // e.g., "ai"
    std::string model_name;      // e.g., "smollm2"
    std::string tag;             // e.g., "135M-Q4_0"
    
    bool is_valid() const {
        return !namespace_name.empty() && !model_name.empty() && !tag.empty();
    }
};

class UrlParser {
public:
    static ModelSpec parse_model_spec(const std::string& model_spec);
    static std::string build_download_url(const ModelSpec& spec);
    static bool is_valid_model_spec(const std::string& model_spec);

private:
    static const std::string REGISTRY_BASE_URL;
    static const std::string MEDIA_TYPE;
};

#endif // URL_PARSER_H