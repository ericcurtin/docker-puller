cmake_minimum_required(VERSION 3.16)
project(docker-ai-puller VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find packages
find_package(PkgConfig REQUIRED)
pkg_check_modules(CURL REQUIRED libcurl)

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${CURL_INCLUDE_DIRS})

# Compile flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CURL_CFLAGS_OTHER}")

# Source files
set(SOURCES
    src/main.cpp
    src/docker_ai_puller.cpp
    src/gguf_validator.cpp
    src/performance_tester.cpp
)

# Create executable
add_executable(docker-ai-puller ${SOURCES})

# Link libraries
target_link_libraries(docker-ai-puller ${CURL_LIBRARIES})

# Install
install(TARGETS docker-ai-puller DESTINATION bin)