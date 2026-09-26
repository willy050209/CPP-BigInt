# benchmarks/CMakeLists.txt

# 1. Target: bench_bigint (Zero-dependency, always builds)
add_executable(bench_bigint
    "${CMAKE_CURRENT_SOURCE_DIR}/cpp/bench_bigint.cpp"
)
target_include_directories(bench_bigint PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../include"
)
target_link_libraries(bench_bigint PRIVATE bigint)

# MSVC optimization flags for benchmarking
if(MSVC)
    target_compile_options(bench_bigint PRIVATE
        /O2
        /Oi
        /Ot
        /Gy
    )
endif()

# 2. Target: bench_boost (Boost.Multiprecision cpp_int)
find_path(BOOST_INCLUDE_DIR NAMES boost/multiprecision/cpp_int.hpp
    PATHS
    "$ENV{BOOST_ROOT}/include"
    "$ENV{BOOST_ROOT}"
    "$ENV{VCPKG_ROOT}/installed/x64-windows/include"
    "C:/vcpkg/installed/x64-windows/include"
    "D:/vcpkg/installed/x64-windows/include"
)

if(BOOST_INCLUDE_DIR)
    message(STATUS "[Benchmark] Found Boost.Multiprecision in ${BOOST_INCLUDE_DIR}")
    add_executable(bench_boost
        "${CMAKE_CURRENT_SOURCE_DIR}/cpp/bench_boost.cpp"
    )
    target_include_directories(bench_boost PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
        "${BOOST_INCLUDE_DIR}"
    )
    if(MSVC)
        target_compile_options(bench_boost PRIVATE /O2 /Oi /Ot /Gy)
    endif()
else()
    message(STATUS "[Benchmark] Boost.Multiprecision not found (optional)")
endif()

# 3. Target: bench_gmp (C++ GMP / MPIR)
find_path(GMP_INCLUDE_DIR NAMES gmpxx.h gmp.h
    PATHS
    "${CMAKE_CURRENT_SOURCE_DIR}/deps/mpir/include"
    "$ENV{GMP_ROOT}/include"
    "$ENV{GMP_ROOT}"
    "$ENV{VCPKG_ROOT}/installed/x64-windows/include"
    "C:/vcpkg/installed/x64-windows/include"
    "D:/vcpkg/installed/x64-windows/include"
)

find_library(GMP_LIB NAMES gmpxx gmp mpir libgmpxx libgmp libmpir
    PATHS
    "${CMAKE_CURRENT_SOURCE_DIR}/deps/mpir/lib"
    "$ENV{GMP_ROOT}/lib"
    "$ENV{GMP_ROOT}"
    "$ENV{VCPKG_ROOT}/installed/x64-windows/lib"
    "C:/vcpkg/installed/x64-windows/lib"
    "D:/vcpkg/installed/x64-windows/lib"
)

if(GMP_INCLUDE_DIR AND GMP_LIB)
    message(STATUS "[Benchmark] Found GMP in ${GMP_INCLUDE_DIR}, lib: ${GMP_LIB}")
    add_executable(bench_gmp
        "${CMAKE_CURRENT_SOURCE_DIR}/cpp/bench_gmp.cpp"
    )
    target_include_directories(bench_gmp PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
        "${GMP_INCLUDE_DIR}"
    )
    target_link_libraries(bench_gmp PRIVATE ${GMP_LIB})
    if(MSVC)
        target_compile_options(bench_gmp PRIVATE /O2 /Oi /Ot /Gy)
    endif()
else()
    message(STATUS "[Benchmark] GMP not found (optional)")
endif()

# 4. Target: bench_layer1_kernels (Pure Arithmetic Kernels)
add_executable(bench_layer1_kernels
    "${CMAKE_CURRENT_SOURCE_DIR}/cpp/bench_layer1_kernels.cpp"
)
target_include_directories(bench_layer1_kernels PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../include"
)
target_link_libraries(bench_layer1_kernels PRIVATE bigint)
if(GMP_INCLUDE_DIR AND GMP_LIB)
    target_include_directories(bench_layer1_kernels PRIVATE "${GMP_INCLUDE_DIR}")
    target_link_libraries(bench_layer1_kernels PRIVATE ${GMP_LIB})
endif()
if(MSVC)
    target_compile_options(bench_layer1_kernels PRIVATE /O2 /Oi /Ot /Gy)
endif()

# 5. Target: bench_layer3_overhead (Allocation & Representation Cost Decomposition)
add_executable(bench_layer3_overhead
    "${CMAKE_CURRENT_SOURCE_DIR}/cpp/bench_layer3_overhead.cpp"
)
target_include_directories(bench_layer3_overhead PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../include"
)
target_link_libraries(bench_layer3_overhead PRIVATE bigint)
if(MSVC)
    target_compile_options(bench_layer3_overhead PRIVATE /O2 /Oi /Ot /Gy)
endif()

# 6. Target: bench_layer4_scaling (Algorithm Crossover & Asymptotic Scaling)
add_executable(bench_layer4_scaling
    "${CMAKE_CURRENT_SOURCE_DIR}/cpp/bench_layer4_scaling.cpp"
)
target_include_directories(bench_layer4_scaling PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../include"
)
target_link_libraries(bench_layer4_scaling PRIVATE bigint)
if(MSVC)
    target_compile_options(bench_layer4_scaling PRIVATE /O2 /Oi /Ot /Gy)
endif()
