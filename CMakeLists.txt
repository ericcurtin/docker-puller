cmake_minimum_required(VERSION 3.12)
project(docker-puller VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(PkgConfig REQUIRED)
pkg_check_modules(CURL REQUIRED libcurl)
pkg_check_modules(JSONCPP REQUIRED jsoncpp)

# Add executable
add_executable(docker-puller
    src/main.cpp
    src/docker_registry.cpp
    src/downloader.cpp
    src/gguf_validator.cpp
    src/utils.cpp
)

# Include directories
target_include_directories(docker-puller PRIVATE
    include
    ${CURL_INCLUDE_DIRS}
    ${JSONCPP_INCLUDE_DIRS}
)

# Link libraries
target_link_libraries(docker-puller PRIVATE
    ${CURL_LIBRARIES}
    ${JSONCPP_LIBRARIES}
)

# Compiler flags
target_compile_options(docker-puller PRIVATE
    ${CURL_CFLAGS_OTHER}
    ${JSONCPP_CFLAGS_OTHER}
    -Wall
    -Wextra
    -Werror
)

# Install target
install(TARGETS docker-puller DESTINATION bin)

# Enable testing
enable_testing()

# Add test executable
add_executable(test_basic
    test/test_basic.cpp
    src/docker_registry.cpp
    src/gguf_validator.cpp
    src/utils.cpp
)

target_include_directories(test_basic PRIVATE
    include
    ${CURL_INCLUDE_DIRS}
    ${JSONCPP_INCLUDE_DIRS}
)

target_link_libraries(test_basic PRIVATE
    ${CURL_LIBRARIES}
    ${JSONCPP_LIBRARIES}
)

target_compile_options(test_basic PRIVATE
    ${CURL_CFLAGS_OTHER}
    ${JSONCPP_CFLAGS_OTHER}
    -Wall
    -Wextra
)

# Add test
add_test(NAME basic_tests COMMAND test_basic)