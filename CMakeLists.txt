cmake_minimum_required(VERSION 3.20)
project(GrowTentController VERSION 0.1.0 LANGUAGES CXX)

# 1. Enforce strict Modern C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Export compile_commands.json so VS Code IntelliSense / clangd works automatically
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 2. Pure Header-Only Core Domain Target
add_library(grow_core INTERFACE)
target_include_directories(grow_core INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/core/include
)

# 3. Enable strict compiler warnings (catches implicit type conversions early)
if(MSVC)
    target_compile_options(grow_core INTERFACE /W4 /WX)
else()
    target_compile_options(grow_core INTERFACE -Wall -Wextra -Wpedantic -Wconversion)
endif()

# 4. Automatically download & configure GoogleTest for Unit Testing
include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()

# 5. Build the Milestone 1 Unit Test executable
add_executable(unit_tests
    tests/test_milestone1.cpp
)
target_link_libraries(unit_tests PRIVATE
    grow_core
    GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(unit_tests)
