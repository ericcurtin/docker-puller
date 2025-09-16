cmake_minimum_required(VERSION 3.10)
project(docker-puller VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(PkgConfig REQUIRED)
pkg_check_modules(CURL REQUIRED libcurl)

# Add compiler flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -O2")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0")

# Include directories
include_directories(${CURL_INCLUDE_DIRS})
include_directories(include)

# Source files
set(SOURCES
    src/main.cpp
    src/docker_puller.cpp
    src/url_resolver.cpp
    src/progress_tracker.cpp
)

# Create executable
add_executable(docker-puller ${SOURCES})

# Link libraries
target_link_libraries(docker-puller ${CURL_LIBRARIES})
target_compile_options(docker-puller PRIVATE ${CURL_CFLAGS_OTHER})

# Install target
install(TARGETS docker-puller DESTINATION bin)