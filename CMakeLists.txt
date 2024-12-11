cmake_minimum_required(VERSION 3.22)
project(WDSYMYS)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_BUILD_TYPE Debug)

add_compile_options(-O3)
#add_compile_options(-g -O0 -fsanitize=address,undefined)
#add_link_options(-fsanitize=address,undefined)

# Make LLVM_PATH configurable
set(LLVM_PATH "" CACHE PATH "Path to LLVM installation (e.g., /usr/lib/llvm or /opt/llvm)")

if(NOT LLVM_PATH)
    message(FATAL_ERROR "LLVM_PATH must be set. Use -DLLVM_PATH=<path> when running CMake.")
endif()

find_package(LLVM REQUIRED CONFIG PATHS ${LLVM_PATH})
list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
include(AddLLVM)

separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND ${LLVM_DEFINITIONS})
add_definitions(${LLVM_DEFINITIONS_LIST})
include_directories(${LLVM_INCLUDE_DIRS})

# Detect shared library extension based on OS
if(APPLE)
    set(SHARED_LIB_EXTENSION ".dylib")
elseif(UNIX)
    set(SHARED_LIB_EXTENSION ".so")
else()
    message(FATAL_ERROR "Unsupported platform. This project supports macOS and Linux only.")
endif()

# Set plugin path using the detected extension
set(WDSYMYS_PLUGIN_PATH ${CMAKE_BINARY_DIR}/wdsymys/WDSYMYS${SHARED_LIB_EXTENSION})

# Locate LLVM tools
list(APPEND CMAKE_PROGRAM_PATH ${LLVM_PATH}/bin)
find_program(LLVM_OPT_EXECUTABLE opt PATHS ${LLVM_PATH}/bin)
find_program(LLVM_CLANG_EXECUTABLE clang PATHS ${LLVM_PATH}/bin)

if(NOT LLVM_OPT_EXECUTABLE)
    message(FATAL_ERROR "opt executable not found. Ensure LLVM_PATH is correct.")
endif()

if(NOT LLVM_CLANG_EXECUTABLE)
    message(FATAL_ERROR "clang executable not found. Ensure LLVM_PATH is correct.")
endif()

message(STATUS "OPT: ${LLVM_OPT_EXECUTABLE}")
message(STATUS "CLANG: ${LLVM_CLANG_EXECUTABLE}")

add_subdirectory(wdsymys)
add_subdirectory(testcases)
