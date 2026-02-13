cmake_minimum_required(VERSION 3.10)

# Project name and version
project(kirin VERSION 0.2 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Compiler flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -march=native -flto")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0 -DDEBUG")

# Default to Release build if not specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Source files
set(SOURCES
    src/main.cpp
    src/types.cpp
    src/bitboard.cpp
    src/utils.cpp
    src/zobrist.cpp
    src/attacks.cpp
    src/movegen.cpp
    src/position.cpp
    src/evaluation.cpp
    src/search.cpp
    src/uci.cpp
)

# Header files (for IDE support)
set(HEADERS
    src/engine.h
    src/types.h
    src/bitboard.h
    src/utils.h
    src/zobrist.h
    src/attacks.h
    src/movegen.h
    src/position.h
    src/evaluation.h
    src/search.h
    src/uci.h
)

# Create executable
add_executable(kirin ${SOURCES} ${HEADERS})

# Include directories
target_include_directories(kirin PRIVATE ${CMAKE_SOURCE_DIR}/src)

# Print build type
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
