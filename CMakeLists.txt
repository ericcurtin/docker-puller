cmake_minimum_required(VERSION 3.12)
project(docker-puller VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(CURL REQUIRED)
find_package(nlohmann_json REQUIRED)

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# Source files
set(SOURCES
    src/main.cpp
    src/docker_puller.cpp
    src/gguf_validator.cpp
)

# Headers
set(HEADERS
    include/docker_puller.hpp
    include/gguf_validator.hpp
)

# Create executable
add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

# Link libraries
target_link_libraries(${PROJECT_NAME} 
    CURL::libcurl
    nlohmann_json::nlohmann_json
    pthread
)

# Compiler options
target_compile_options(${PROJECT_NAME} PRIVATE
    -Wall -Wextra -O3 -g
)

# Install target
install(TARGETS ${PROJECT_NAME} DESTINATION bin)