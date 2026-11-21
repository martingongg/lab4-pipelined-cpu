cmake_minimum_required(VERSION 3.16)
project(translation_perf_model VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

add_library(perf_model_core STATIC
  src/json.cpp
  src/config.cpp
  src/dtlb.cpp
  src/memory.cpp
  src/walker.cpp
  src/arbiter.cpp
  src/retire.cpp
  src/workloads.cpp
  src/stats.cpp
  src/simulator.cpp
)

target_include_directories(perf_model_core PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_options(perf_model_core PRIVATE
  -Wall -Wextra -Wpedantic
)

add_executable(translation_sim
  src/main.cpp
)

target_link_libraries(translation_sim PRIVATE perf_model_core)
target_compile_options(translation_sim PRIVATE -Wall -Wextra -Wpedantic)

# Testing (GoogleTest + Python reference + Golden regression via CTest)
include(CTest)
if(BUILD_TESTING)
  find_package(GTest QUIET)
  if(NOT GTest_FOUND)
    include(FetchContent)
    FetchContent_Declare(
      googletest
      URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.zip
    )
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
  endif()

  add_executable(perf_model_tests
    tests/test_perf_model.cpp
  )
  target_link_libraries(perf_model_tests PRIVATE perf_model_core GTest::gtest_main)
  target_compile_options(perf_model_tests PRIVATE -Wall -Wextra -Wpedantic)

  include(GoogleTest)
  gtest_discover_tests(perf_model_tests)

  find_package(Python3 COMPONENTS Interpreter REQUIRED)

  add_test(
    NAME python_lru_reference
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/scripts/ref_tlb.py
            --binary $<TARGET_FILE:translation_sim>
            --verify-suite
  )

  add_test(
    NAME golden_regression
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/scripts/regress.py
            --binary $<TARGET_FILE:translation_sim>
            --configs-dir ${CMAKE_CURRENT_SOURCE_DIR}/configs
            --golden-dir ${CMAKE_CURRENT_SOURCE_DIR}/results/golden
  )
endif()
