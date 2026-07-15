cmake_minimum_required(VERSION 3.18)

# Lightweight, dependency-free tests that exercise the pure-logic helpers
# (hex, target comparison). They compile a couple of leaf sources standalone.
add_executable(test_moneymaker
    test_main.cpp
    ${CMAKE_SOURCE_DIR}/src/common/util.cpp
    ${CMAKE_SOURCE_DIR}/src/core/job.cpp
)

target_include_directories(test_moneymaker PRIVATE ${CMAKE_SOURCE_DIR}/src)

enable_testing()
add_test(NAME basic COMMAND test_moneymaker)
