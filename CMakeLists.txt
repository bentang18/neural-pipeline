cmake_minimum_required(VERSION 3.23)

project(neural-pipeline VERSION 0.0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Library target
add_library(neural_pipeline
src/science/neural_pipeline/pipeline.cpp
src/science/neural_pipeline/producer.cpp
src/science/neural_pipeline/processor.cpp
src/science/neural_pipeline/socket_transport.cpp)

target_include_directories(neural_pipeline
  PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)

# Google Test via FetchContent
include(FetchContent)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG v1.15.2
)
FetchContent_MakeAvailable(googletest)

# Test executable
add_executable(neural_pipeline_tests
  test/test_ring_buffer.cpp
  test/test_socket_transport.cpp
)
target_link_libraries(neural_pipeline_tests
  PRIVATE neural_pipeline gtest_main
)

# Example executables
add_executable(pipeline_demo
  examples/main.cpp
)
target_link_libraries(pipeline_demo
  PRIVATE neural_pipeline
)

add_executable(producer_node
  examples/producer_node.cpp
)
target_link_libraries(producer_node
  PRIVATE neural_pipeline
)

add_executable(processor_node
  examples/processor_node.cpp
)
target_link_libraries(processor_node
  PRIVATE neural_pipeline
)
