cmake_minimum_required(VERSION 3.12)
project(docker-puller VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(PkgConfig REQUIRED)
pkg_check_modules(CURL REQUIRED libcurl)

# Add executable
add_executable(docker-puller
    src/main.cpp
    src/docker_puller.cpp
    src/url_parser.cpp
)

# Include directories
target_include_directories(docker-puller PRIVATE
    include
    ${CURL_INCLUDE_DIRS}
)

# Link libraries
target_link_libraries(docker-puller
    ${CURL_LIBRARIES}
    pthread
)

# Compiler flags
target_compile_options(docker-puller PRIVATE
    ${CURL_CFLAGS_OTHER}
    -Wall
    -Wextra
    -O3
)

# Installation
install(TARGETS docker-puller DESTINATION bin)