cmake_minimum_required(VERSION 3.15)
project(CPP_BigInt VERSION 1.0.0 LANGUAGES CXX)

if(NOT DEFINED CMAKE_CXX_STANDARD)
    set(CMAKE_CXX_STANDARD 20)
endif()

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# HEADER-ONLY interface target
add_library(bigint INTERFACE)
target_include_directories(bigint INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# Compiler warning flags
if(MSVC)
    target_compile_options(bigint INTERFACE
        /W4
        /utf-8
        /Zc:__cplusplus
    )
else()
    target_compile_options(bigint INTERFACE
        -Wall
        -Wextra
    )
endif()

# Testing target: bigint_test (tests/*.cpp)
enable_testing()

file(GLOB TEST_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp")
if(TEST_SOURCES)
    add_executable(bigint_test ${TEST_SOURCES})
    target_link_libraries(bigint_test PRIVATE bigint)
    add_test(NAME bigint_unit_tests COMMAND bigint_test)
endif()

# Benchmarking targets
option(BIGINT_BUILD_BENCHMARKS "Build benchmark suite" OFF)
if(BIGINT_BUILD_BENCHMARKS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/benchmarks/CMakeLists.txt")
    add_subdirectory(benchmarks)
endif()

