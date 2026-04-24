cmake_minimum_required(VERSION 3.10)

# ══════════════════════════════════════════════════════════════════════════════
# KIRIN - Autonomous Chess System
# ══════════════════════════════════════════════════════════════════════════════
# Combined build for chess engine + physical board control
#
# Directory Structure:
#   kirin/
#   ├── CMakeLists.txt          (this file)
#   ├── src/
#   │   ├── engine/             Chess engine core
#   │   │   ├── attacks.cpp/h
#   │   │   ├── bitboard.cpp/h
#   │   │   ├── engine.h
#   │   │   ├── evaluation.cpp/h
#   │   │   ├── movegen.cpp/h
#   │   │   ├── position.cpp/h
#   │   │   ├── search.cpp/h
#   │   │   ├── types.cpp/h
#   │   │   ├── uci.cpp/h
#   │   │   ├── utils.cpp/h
#   │   │   └── zobrist.cpp/h
#   │   │
#   │   ├── hardware/           Physical board control
#   │   │   ├── board_interpreter.cpp/h
#   │   │   ├── board_scanner.cpp/h
#   │   │   ├── button_controller.cpp/h
#   │   │   ├── display_controller.cpp/h
#   │   │   ├── game_controller.cpp/h
#   │   │   └── gantry_controller.cpp/h
#   │   │
#   │   └── main.cpp            Entry point
#   │
#   └── tests/
#       ├── captured_piece_test.cpp
#       ├── board_interpreter_test.cpp
#       ├── game_controller_test.cpp
#       ├── engine_test.cpp
#       ├── board_scanner_test.cpp
#       ├── ui_controller_test.cpp
#       ├── generate_test_report.py   <- produces TEST_RESULTS.md
#       └── TEST_RESULTS.md           <- auto-generated, commit to repo
#
# ══════════════════════════════════════════════════════════════════════════════

project(kirin VERSION 0.3 LANGUAGES CXX)

# ============================================
# Compiler Configuration
# ============================================
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -march=native -flto")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0 -DDEBUG")

# Default to Release build if not specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")

option(KIRIN_ENABLE_GPIOD "Enable Raspberry Pi GPIO support through libgpiod v2" OFF)

# Backward compatibility for the old build style:
#   cmake .. -DCMAKE_CXX_FLAGS="-DHAS_GPIOD"
# or:
#   cmake .. -DHAS_GPIOD=ON
if(CMAKE_CXX_FLAGS MATCHES "HAS_GPIOD" OR (DEFINED HAS_GPIOD AND HAS_GPIOD))
    set(KIRIN_ENABLE_GPIOD ON)
endif()

# ============================================
# Source Files - Engine
# ============================================
set(ENGINE_SOURCES
    src/engine/types.cpp
    src/engine/bitboard.cpp
    src/engine/utils.cpp
    src/engine/zobrist.cpp
    src/engine/attacks.cpp
    src/engine/movegen.cpp
    src/engine/position.cpp
    src/engine/evaluation.cpp
    src/engine/search.cpp
    src/engine/uci.cpp
)

set(ENGINE_HEADERS
    src/engine/engine.h
    src/engine/types.h
    src/engine/bitboard.h
    src/engine/utils.h
    src/engine/zobrist.h
    src/engine/attacks.h
    src/engine/movegen.h
    src/engine/position.h
    src/engine/evaluation.h
    src/engine/search.h
    src/engine/uci.h
)

# ============================================
# Source Files - Hardware Control
# ============================================
set(HARDWARE_SOURCES
    src/hardware/board_interpreter.cpp
    src/hardware/board_scanner.cpp
    src/hardware/button_controller.cpp
    src/hardware/display_controller.cpp
    src/hardware/gantry_controller.cpp
    src/hardware/game_controller.cpp
)

set(HARDWARE_HEADERS
    src/hardware/board_interpreter.h
    src/hardware/board_scanner.h
    src/hardware/button_controller.h
    src/hardware/display_controller.h
    src/hardware/gantry_controller.h
    src/hardware/game_controller.h
)

# ============================================
# Libraries
# ============================================

# Engine library (for linking with tests and hardware)
add_library(kirin_engine STATIC ${ENGINE_SOURCES} ${ENGINE_HEADERS})
target_include_directories(kirin_engine PUBLIC ${CMAKE_SOURCE_DIR}/src/engine)

# Hardware library (depends on engine for game_controller)
add_library(kirin_hardware STATIC ${HARDWARE_SOURCES} ${HARDWARE_HEADERS})
target_include_directories(kirin_hardware PUBLIC 
    ${CMAKE_SOURCE_DIR}/src/hardware
    ${CMAKE_SOURCE_DIR}/src/engine
)
target_link_libraries(kirin_hardware PUBLIC kirin_engine)

# Link libgpiod when GPIO support is enabled.
if(KIRIN_ENABLE_GPIOD)
    find_library(GPIOD_LIB NAMES gpiod)
    if(NOT GPIOD_LIB)
        message(FATAL_ERROR "KIRIN_ENABLE_GPIOD=ON but libgpiod was not found")
    endif()

    target_compile_definitions(kirin_hardware PUBLIC HAS_GPIOD)
    target_link_libraries(kirin_hardware PUBLIC ${GPIOD_LIB})
    message(STATUS "GPIO support enabled with libgpiod: ${GPIOD_LIB}")
else()
    message(STATUS "GPIO support disabled; scanner/buttons require simulation mode")
endif()

# ============================================
# Main Executable
# ============================================
add_executable(kirin src/main.cpp)
target_include_directories(kirin PRIVATE 
    ${CMAKE_SOURCE_DIR}/src/engine
    ${CMAKE_SOURCE_DIR}/src/hardware
)
target_link_libraries(kirin PRIVATE kirin_engine kirin_hardware)



# ============================================
# Testing
# ============================================
enable_testing()

# Captured piece detection test (engine-only)
add_executable(captured_piece_test tests/captured_piece_test.cpp)
target_link_libraries(captured_piece_test PRIVATE kirin_engine)
add_test(NAME CapturedPieceTest COMMAND captured_piece_test)

# Board interpreter test (hardware-only, standalone)
add_executable(board_interpreter_test tests/board_interpreter_test.cpp)
target_link_libraries(board_interpreter_test PRIVATE kirin_hardware)
add_test(NAME BoardInterpreterTest COMMAND board_interpreter_test)

# Game controller test (integration layer - no hardware required)
add_executable(game_controller_test tests/game_controller_test.cpp)
target_include_directories(game_controller_test PRIVATE
    ${CMAKE_SOURCE_DIR}/src/engine
    ${CMAKE_SOURCE_DIR}/src/hardware
)
target_link_libraries(game_controller_test PRIVATE kirin_hardware kirin_engine)
add_test(NAME GameControllerTest COMMAND game_controller_test)

# Engine validity and skill level test (engine-only)
add_executable(engine_test tests/engine_test.cpp)
target_include_directories(engine_test PRIVATE
    ${CMAKE_SOURCE_DIR}/src/engine
)
target_link_libraries(engine_test PRIVATE kirin_engine)
add_test(NAME EngineTest COMMAND engine_test)

# Board scanner test (sensor move detection logic - no hardware required)
add_executable(board_scanner_test tests/board_scanner_test.cpp)
target_include_directories(board_scanner_test PRIVATE
    ${CMAKE_SOURCE_DIR}/src/engine
    ${CMAKE_SOURCE_DIR}/src/hardware
)
target_link_libraries(board_scanner_test PRIVATE kirin_hardware kirin_engine)
add_test(NAME BoardScannerTest COMMAND board_scanner_test)

# UI controller test (OLED/button logic - no hardware required)
add_executable(ui_controller_test tests/ui_controller_test.cpp)
target_include_directories(ui_controller_test PRIVATE
    ${CMAKE_SOURCE_DIR}/src/hardware
)
target_link_libraries(ui_controller_test PRIVATE kirin_hardware)
add_test(NAME UIControllerTest COMMAND ui_controller_test)

# ============================================
# Test Report (Markdown for GitHub)
# ============================================
# Runs all test binaries and writes tests/TEST_RESULTS.md
# Usage: cmake --build build --target test_report
add_custom_target(test_report
    COMMAND ${CMAKE_COMMAND} -E echo "Generating test report..."
    COMMAND python3 ${CMAKE_SOURCE_DIR}/tests/generate_test_report.py
            --build-dir ${CMAKE_BINARY_DIR}
            --output    ${CMAKE_SOURCE_DIR}/tests/TEST_RESULTS.md
    DEPENDS
        captured_piece_test
        board_interpreter_test
        game_controller_test
        engine_test
        board_scanner_test
        ui_controller_test
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Running all tests and writing tests/TEST_RESULTS.md"
)

# ============================================
# Optional: Install targets
# ============================================
install(TARGETS kirin DESTINATION bin)

# ============================================
# Summary
# ============================================
message(STATUS "")
message(STATUS "=== KIRIN Build Configuration ===")
message(STATUS "Engine sources:   ${CMAKE_SOURCE_DIR}/src/engine/")
message(STATUS "Hardware sources: ${CMAKE_SOURCE_DIR}/src/hardware/")
message(STATUS "Tests:            ${CMAKE_SOURCE_DIR}/tests/")
message(STATUS "  captured_piece_test, board_interpreter_test,")
message(STATUS "  game_controller_test, engine_test")
message(STATUS "=================================")
message(STATUS "")
