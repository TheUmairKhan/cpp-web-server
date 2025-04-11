cmake_minimum_required(VERSION 3.10.0)
project(lebron2016x4)

# ─────────────────────────────────────────────────────────────
#  Guard against in‑source builds
# ─────────────────────────────────────────────────────────────
if (${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})
    message(FATAL_ERROR "In‑source builds not allowed.\n"
        "Create a separate build directory and run CMake from there.")
endif()

# Default to Debug inside the class dev shell
if (CMAKE_BUILD_TYPE STREQUAL "" AND "$ENV{DEVEL_SHELL}" STREQUAL "1")
    message(STATUS "Setting Debug build type by default in devel shell")
    set(CMAKE_BUILD_TYPE Debug)
endif()

# Place binaries under build/bin/
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# ─────────────────────────────────────────────────────────────
#  Testing infrastructure (optional for now)
# ─────────────────────────────────────────────────────────────
include(CTest)
enable_testing()

include(GoogleTest)
add_subdirectory(/usr/src/googletest googletest)

# ─────────────────────────────────────────────────────────────
#  Boost
# ─────────────────────────────────────────────────────────────
set(Boost_USE_STATIC_LIBS ON)
find_package(Boost 1.50 REQUIRED COMPONENTS system)
message(STATUS "Boost version: ${Boost_VERSION}")

# ─────────────────────────────────────────────────────────────
#  Echo‑server static library
# ─────────────────────────────────────────────────────────────
add_library(echoserver_lib
  src/session.cc
  src/server.cc
  src/config_parser.cc
)

target_include_directories(echoserver_lib
  PUBLIC                     # headers are part of the public interface
    ${CMAKE_SOURCE_DIR}/include
)

target_compile_definitions(echoserver_lib
  PRIVATE
    BOOST_BIND_GLOBAL_PLACEHOLDERS
)

target_link_libraries(echoserver_lib
  PUBLIC                     # expose Boost::system to users of the lib
    Boost::system
)

# ─────────────────────────────────────────────────────────────
#  Executable that links the library
# ─────────────────────────────────────────────────────────────
add_executable(webserver     # <-- rename from 'server' if you like
  src/server_main.cc         # (contains the config‑file logic)
)

target_include_directories(webserver
  PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(webserver
  PRIVATE
    echoserver_lib           # our library
    Boost::system            # optional explicit link
)

# ─────────────────────────────────────────────────────────────
#  (Tests & coverage placeholders)
# ─────────────────────────────────────────────────────────────
add_executable(server_test
  tests/server_test.cc
)

target_include_directories(server_test
  PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(server_test
  PRIVATE
    echoserver_lib
    gtest_main
    Boost::system
)

# Let CTest discover and run the tests automatically
gtest_discover_tests(server_test
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/tests
)
# TODO(!): Update with target/test targets
# include(cmake/CodeCoverageReportConfig.cmake)
# generate_coverage_report(TARGETS example_server example_lib TESTS example_lib_test)
