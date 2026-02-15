cmake_minimum_required(VERSION 3.16)
project(KirinBoardInterpreter VERSION 0.1 LANGUAGES CXX)

# C++17 standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Compiler warnings
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Build type (default to Release)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Board Interpreter Library
add_library(board_interpreter STATIC
    src/board_interpreter.cpp
    src/board_interpreter.h
)

target_include_directories(board_interpreter PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Test executable
add_executable(test_board_interpreter
    src/board_interpreter_test.cpp
)

target_link_libraries(test_board_interpreter PRIVATE board_interpreter)

# Optional: Add a simple "run test" target
add_custom_target(run_test
    COMMAND test_board_interpreter
    DEPENDS test_board_interpreter
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Running board interpreter tests..."
)

# Print build info
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
