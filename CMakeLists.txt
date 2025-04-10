cmake_minimum_required(VERSION 3.10.0)
project(lebron2016x4)

# Prevent in-source builds
if (${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})
    message(FATAL_ERROR "In-source builds not allowed.\n"
        "Please make a new directory (called a build directory) and run CMake from there.\n"
        "You may need to remove CMakeCache.txt.")
endif()

# Default to Debug in devel shell
if (CMAKE_BUILD_TYPE STREQUAL "" AND "$ENV{DEVEL_SHELL}" STREQUAL "1")
    message(STATUS "Setting debug build type by default in devel shell")
    set(CMAKE_BUILD_TYPE Debug)
endif()

# Output binaries to bin/
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Enable testing
include(CTest)
enable_testing()

# GoogleTest (optional)
include(GoogleTest)
add_subdirectory(/usr/src/googletest googletest)

# Boost (static)
set(Boost_USE_STATIC_LIBS ON)
find_package(Boost 1.50 REQUIRED COMPONENTS system)
message(STATUS "Boost version: ${Boost_VERSION}")

# === Echo Server Library ===
# Build a static library from session and server implementations
add_library(echoserver_lib
  src/session.cc
  src/server.cc
)

# Make include/ available for the library
target_include_directories(echoserver_lib
  PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

# Workaround for Boost.Bind placeholder warning
target_compile_definitions(echoserver_lib
  PRIVATE
    BOOST_BIND_GLOBAL_PLACEHOLDERS
)

# Link against Boost.System for Asio
target_link_libraries(echoserver_lib
  PUBLIC
    Boost::system
)

# === Server Executable ===
# Build main that links to the library
add_executable(server
  src/server_main.cc
)

target_include_directories(server
  PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

# Link the echo server library into the executable
target_link_libraries(server
  PRIVATE
    echoserver_lib
)

# IGNORE BELOW FOR NOW:
# TODO(!): Update test executable name, srcs, and deps
# add_executable(example_lib_test tests/example_lib_test.cc)
# target_link_libraries(example_lib_test example_lib gtest_main)

# TODO(!): Update with test binary
# gtest_discover_tests(example_lib_test WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/tests)

# TODO(!): Update with target/test targets
# include(cmake/CodeCoverageReportConfig.cmake)
# generate_coverage_report(TARGETS example_server example_lib TESTS example_lib_test)
