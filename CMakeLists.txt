cmake_minimum_required(VERSION 3.18)

project(moneymaker
    VERSION 0.1.0
    DESCRIPTION "Moneymaker - cross-platform unified CPU+GPU crypto miner"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

option(WITH_CUDA    "Build NVIDIA CUDA backend (KawPow)" ON)
option(WITH_OPENCL  "Build AMD OpenCL backend (KawPow)" ON)
option(BUILD_TESTS  "Build unit tests" OFF)

# ---------------------------------------------------------------------------
# Vendored third-party libraries
# ---------------------------------------------------------------------------
# RandomX (CPU RandomX / GhostRider stage 1)
add_subdirectory(third_party/RandomX EXCLUDE_FROM_ALL)
# ethash (Keccak + DAG used by KawPow)
set(ETHASH_BUILD_ETHASH ON CACHE BOOL "" FORCE)
set(ETHASH_BUILD_GLOBAL_CONTEXT OFF CACHE BOOL "" FORCE)
set(ETHASH_TESTING OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/ethash EXCLUDE_FROM_ALL)
# nlohmann/json (single-header)
add_library(nlohmann_json INTERFACE)
target_include_directories(nlohmann_json INTERFACE ${CMAKE_SOURCE_DIR}/third_party/json/include)
# spdlog (header-only)
add_library(spdlog INTERFACE)
target_include_directories(spdlog INTERFACE ${CMAKE_SOURCE_DIR}/third_party/spdlog/include)

# ---------------------------------------------------------------------------
# Core sources (always built)
# ---------------------------------------------------------------------------
set(MONEYMAKER_SOURCES
    src/main.cpp
    src/common/log.cpp
    src/common/util.cpp
    src/common/socket.cpp
    src/config/config.cpp
    src/core/miner.cpp
    src/core/job.cpp
    src/core/backend.cpp
    src/stratum/stratum_client.cpp
    src/stratum/stratum_protocol.cpp
    src/benchmark/benchmark.cpp
    src/algo/randomx.cpp
    src/algo/kawpow.cpp
    src/algo/kawpow_dag.cpp
    src/algo/ghostrider.cpp
    src/backend/cpu/cpu_backend.cpp
)

add_executable(moneymaker ${MONEYMAKER_SOURCES})

target_include_directories(moneymaker PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/third_party/RandomX/src
)

target_link_libraries(moneymaker PRIVATE
    randomx
    ethash::ethash
    nlohmann_json
    spdlog
)

find_package(Threads REQUIRED)
target_link_libraries(moneymaker PRIVATE Threads::Threads)

if(WIN32)
    target_link_libraries(moneymaker PRIVATE ws2_32)
endif()

target_compile_options(moneymaker PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra>
    $<$<CXX_COMPILER_ID:MSVC>:/W3>
)

# ---------------------------------------------------------------------------
# Optional CUDA backend (NVIDIA)
# ---------------------------------------------------------------------------
if(WITH_CUDA)
    find_package(CUDAToolkit QUIET)
    if(CUDAToolkit_FOUND)
        enable_language(CUDA)
        target_sources(moneymaker PRIVATE
            src/backend/cuda/cuda_backend.cpp
            src/backend/cuda/kawpow.cu
        )
        target_compile_definitions(moneymaker PRIVATE HAVE_CUDA=1)
        target_link_libraries(moneymaker PRIVATE CUDA::cudart)
        message(STATUS "Moneymaker: CUDA backend ENABLED")
    else()
        message(STATUS "Moneymaker: CUDA toolkit not found - CUDA backend disabled")
    endif()
endif()

# ---------------------------------------------------------------------------
# Optional OpenCL backend (AMD / portable)
# ---------------------------------------------------------------------------
if(WITH_OPENCL)
    find_package(OpenCL QUIET)
    if(OpenCL_FOUND)
        target_sources(moneymaker PRIVATE
            src/backend/opencl/opencl_backend.cpp
        )
        target_compile_definitions(moneymaker PRIVATE HAVE_OPENCL=1)
        target_link_libraries(moneymaker PRIVATE ${OpenCL_LIBRARIES})
        target_include_directories(moneymaker PRIVATE ${OpenCL_INCLUDE_DIRS})
        message(STATUS "Moneymaker: OpenCL backend ENABLED")
    else()
        message(STATUS "Moneymaker: OpenCL not found - OpenCL backend disabled")
    endif()
endif()

# ---------------------------------------------------------------------------
# Tests (optional)
# ---------------------------------------------------------------------------
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

install(TARGETS moneymaker RUNTIME DESTINATION bin)
install(FILES config.json DESTINATION etc/moneymaker OPTIONAL)
