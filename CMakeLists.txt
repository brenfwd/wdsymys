cmake_minimum_required(VERSION 3.22)
project(WDSYMYS)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_BUILD_TYPE Debug)

set(LLVM_PATH "/opt/homebrew/opt/llvm")
find_package(LLVM REQUIRED CONFIG PATHS ${LLVM_PATH}) # TODO: brew-independent
list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
include(AddLLVM)

separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND ${LLVM_DEFINITIONS})
add_definitions(${LLVM_DEFINITIONS_LIST})
include_directories(${LLVM_INCLUDE_DIRS})

# TODO: .dylib/.so depending on OS
set(WDSYMYS_PLUGIN_PATH ${CMAKE_BINARY_DIR}/wdsymys/WDSYMYS.dylib)
list(APPEND CMAKE_PROGRAM_PATH ${LLVM_PATH}/bin)
find_program(LLVM_OPT_EXECUTABLE opt)
find_program(LLVM_CLANG_EXECUTABLE clang)

if(NOT LLVM_OPT_EXECUTABLE)
    message(FATAL_ERROR "opt executable not found.")
endif()

if(NOT LLVM_CLANG_EXECUTABLE)
    message(FATAL_ERROR "clang executable not found.")
endif()

message(STATUS "OPT: ${LLVM_OPT_EXECUTABLE}")
message(STATUS "CLANG: ${LLVM_CLANG_EXECUTABLE}")

add_subdirectory(wdsymys)
add_subdirectory(testcases)
