file(GLOB TEST_SRCS CONFIGURE_DEPENDS "*.c")
message(STATUS "TEST_SRCS: ${TEST_SRCS}")

ADD_CUSTOM_COMMAND(
    OUTPUT output.ll
    COMMAND clang -c -Xclang -disable-O0-optnone -emit-llvm ${TEST_SRCS} -o output.ll
    DEPENDS ${TEST_SRCS}
)

add_custom_target(build_llvm ALL DEPENDS output.ll)

# foreach(TEST_SRC TEST_SRCS)
#     cmake_path(GET TEST_SRC STEM TEST_NAME)
#     ADD_CUSTOM_COMMAND(
#         OUTPUT ${TEST_NAME}.ll
#         COMMAND clang -S -emit-llvm ${TEST_SRC} -o  ${TEST_NAME}.ll
#         DEPENDS ${TEST_SRC}
#     )
# endforeach()  