cmake_minimum_required(VERSION 3.20)

# 1. Read Semantic Version from the root VERSION file (Single Source of Truth)
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" GROWTENT_VERSION_STR LIMIT_COUNT 1)
string(STRIP "${GROWTENT_VERSION_STR}" GROWTENT_VERSION_STR)

project(GrowTentController VERSION ${GROWTENT_VERSION_STR} LANGUAGES CXX)

# 2. Extract current Git short commit hash (e.g., "a1b2c3d") for firmware traceability
find_package(Git QUIET)
if(GIT_FOUND AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/.git")
    execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_COMMIT_HASH
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
endif()
if(NOT GIT_COMMIT_HASH)
    set(GIT_COMMIT_HASH "dev")
endif()

# 3. Generate build/generated/domain/Version.hpp from Version.hpp.in
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/core/include/domain/Version.hpp.in"
    "${CMAKE_CURRENT_BINARY_DIR}/generated/domain/Version.hpp"
    @ONLY
)

# 4. Enforce strict Modern C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 5. Pure Header-Only Core Domain Target
add_library(grow_core INTERFACE)
target_include_directories(grow_core INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/core/include
    ${CMAKE_CURRENT_BINARY_DIR}/generated
)

# 6. Strict professional compiler warnings
if(MSVC)
    target_compile_options(grow_core INTERFACE /W4 /WX)
else()
    target_compile_options(grow_core INTERFACE -Wall -Wextra -Wpedantic -Wconversion)
endif()

# 7. Unit Testing with GoogleTest
include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()

add_executable(unit_tests
    tests/test_milestone1.cpp
)
target_link_libraries(unit_tests PRIVATE
    grow_core
    GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(unit_tests)
