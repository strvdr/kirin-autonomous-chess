cmake_minimum_required(VERSION 3.16)
project(Kirin VERSION 0.2 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Compiler flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0")

# Default to Release build if not specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Source files
set(SOURCES
    src/engine.cpp
    src/main.cpp
)

# Create the executable
add_executable(kirin ${SOURCES})

# Include directories
target_include_directories(kirin PRIVATE ${CMAKE_SOURCE_DIR}/src)

# Print build type
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
