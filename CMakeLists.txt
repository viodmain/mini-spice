cmake_minimum_required(VERSION 3.14)
project(mini-spice
    VERSION 0.2.0
    DESCRIPTION "A minimal SPICE circuit simulator (C++17)"
    LANGUAGES CXX
)

# Require C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Default build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Compiler warnings
add_compile_options(
    -Wall
    -Wextra
    -Wpedantic
)

# Release flags
set(CMAKE_CXX_FLAGS_RELEASE "-O2")

# Debug flags
set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g")

#=============================================================================
# Include directories
#=============================================================================
include_directories(${PROJECT_SOURCE_DIR}/include)

#=============================================================================
# Source files
#=============================================================================

# Core
set(SRC_CORE
    src/core/circuit.cpp
)

# Parser
set(SRC_PARSER
    src/parser/parser.cpp
)

# Devices
set(SRC_DEVICES
    src/devices/res.cpp
    src/devices/cap.cpp
    src/devices/ind.cpp
    src/devices/vsrc.cpp
    src/devices/isrc.cpp
    src/devices/vccs.cpp
    src/devices/vcvs.cpp
    src/devices/cccs.cpp
    src/devices/ccvs.cpp
    src/devices/dio.cpp
    src/devices/bjt.cpp
    src/devices/mos.cpp
    src/devices/behsrc.cpp
    src/devices/switch.cpp
    src/devices/tline.cpp
    src/devices/devreg.cpp
)

# Analysis
set(SRC_ANALYSIS
    src/analysis/dcop.cpp
    src/analysis/dcsweep.cpp
    src/analysis/acan.cpp
    src/analysis/dctran.cpp
    src/analysis/noise.cpp
    src/analysis/fourier.cpp
    src/analysis/sens.cpp
    src/analysis/anareg.cpp
)

# Math
set(SRC_MATH
    src/math/sparse.cpp
)

# Output
set(SRC_OUTPUT
    src/output/output.cpp
)

# Main
set(SRC_MAIN
    src/main.cpp
)

# All sources
set(SOURCES
    ${SRC_CORE}
    ${SRC_PARSER}
    ${SRC_DEVICES}
    ${SRC_ANALYSIS}
    ${SRC_MATH}
    ${SRC_OUTPUT}
    ${SRC_MAIN}
)

#=============================================================================
# Executable
#=============================================================================
add_executable(mini-spice ${SOURCES})
target_link_libraries(mini-spice PRIVATE m)

#=============================================================================
# Install
#=============================================================================
include(GNUInstallDirs)

install(TARGETS mini-spice
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/mini-spice
    FILES_MATCHING PATTERN "*.hpp"
)

#=============================================================================
# Tests (if test netlists exist)
#=============================================================================
enable_testing()

set(TEST_NETLISTS
    tests/voltage_divider.net
    tests/rc_filter.net
    tests/diode_clipper.net
    tests/rc_transient.net
    tests/bjt_amp.net
    tests/mos_amp.net
)

foreach(test_netlist ${TEST_NETLISTS})
    if(EXISTS ${PROJECT_SOURCE_DIR}/${test_netlist})
        get_filename_component(test_name ${test_netlist} NAME_WE)
        add_test(
            NAME ${test_name}
            COMMAND mini-spice ${PROJECT_SOURCE_DIR}/${test_netlist}
        )
    endif()
endforeach()

#=============================================================================
# Summary
#=============================================================================
message(STATUS "")
message(STATUS "mini-spice ${PROJECT_VERSION}")
message(STATUS "=========================")
message(STATUS "Build type:    ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ compiler:  ${CMAKE_CXX_COMPILER}")
message(STATUS "C++ standard:  ${CMAKE_CXX_STANDARD}")
message(STATUS "Install prefix: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "")
