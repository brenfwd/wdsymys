cmake_minimum_required(VERSION 3.22)
project(WDSYMYS)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_BUILD_TYPE Debug)

find_package(LLVM REQUIRED CONFIG PATHS /opt/homebrew/opt/llvm/) # TODO: brew-independent
list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
include(AddLLVM)

separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND ${LLVM_DEFINITIONS})
add_definitions(${LLVM_DEFINITIONS_LIST})
include_directories(${LLVM_INCLUDE_DIRS})

add_subdirectory(wdsymys)
# add_subdirectory(testcases)

# TODO: .dylib/.so depending on OS
set(WDSYMYS_PLUGIN_PATH ${CMAKE_BINARY_DIR}/wdsymys/WDSYMYS.dylib)
set(INPUT_LL_FILE ${CMAKE_BINARY_DIR}/wdsymys/test/output.ll)
set(OUTPUT_BC_FILE ${CMAKE_BINARY_DIR}/wdsymys/test/output.bc)

find_program(LLVM_OPT_EXECUTABLE opt)

if(NOT LLVM_OPT_EXECUTABLE)
    message(FATAL_ERROR "opt executable not found.")
endif()

message(STATUS "OPT: ${LLVM_OPT_EXECUTABLE}")

# add_custom_command(
#     OUTPUT ${OUTPUT_BC_FILE}
#     COMMAND ${LLVM_OPT_EXECUTABLE}
#         -load-pass-plugin=${WDSYMYS_PLUGIN_PATH}
#         -passes=wdsymys
#         ${INPUT_LL_FILE}
#         -o ${OUTPUT_BC_FILE}
#     DEPENDS ${WDSYMYS_PLUGIN_PATH} ${INPUT_LL_FILE}
#     COMMENT "Running opt with WDSYMYS pass"
#     VERBATIM
# )

# add_custom_target(RunOpt ALL DEPENDS ${OUTPUT_BC_FILE})
# add_dependencies(RunOpt WDSYMYS)
