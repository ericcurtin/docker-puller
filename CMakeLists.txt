cmake_minimum_required(VERSION 3.15)
project(docker-ai-puller)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBCURL REQUIRED libcurl)

# Add nlohmann/json
find_package(nlohmann_json QUIET)
if(NOT nlohmann_json_FOUND)
    include(FetchContent)
    FetchContent_Declare(json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG v3.11.3)
    FetchContent_MakeAvailable(json)
endif()

# Create executable
add_executable(docker-ai-puller
    src/main.cpp
    src/docker_ai_puller.cpp
    src/url_parser.cpp
    src/multi_downloader.cpp
    src/gguf_validator.cpp
)

# Include directories
target_include_directories(docker-ai-puller PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${LIBCURL_INCLUDE_DIRS}
)

# Link libraries
target_link_libraries(docker-ai-puller
    ${LIBCURL_LIBRARIES}
    nlohmann_json::nlohmann_json
)

# Compiler flags
target_compile_options(docker-ai-puller PRIVATE ${LIBCURL_CFLAGS_OTHER})

# Install
install(TARGETS docker-ai-puller DESTINATION bin)