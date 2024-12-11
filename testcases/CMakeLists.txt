set(TESTCASES "")
file(GLOB TEST_SRCS CONFIGURE_DEPENDS "*.c" "*.cpp")
message(STATUS "TEST_SRCS: ${TEST_SRCS}")

set(INITIAL_CC_FLAGS -O0 -Wno-implicit-int -Wno-int-conversion -Wno-implicit-function-declaration)
#set(INITIAL_CC_FLAGS -O3 -Wno-implicit-int -Wno-int-conversion -Wno-implicit-function-declaration)
set(TESTCASE_CC_FLAGS -O3)
# set(TESTCASE_CC_FLAGS -O1)

foreach(TEST_SRC ${TEST_SRCS})
    cmake_path(GET TEST_SRC STEM TEST_NAME)
    message(STATUS "TEST: ${TEST_NAME}")
    ADD_CUSTOM_COMMAND(
        OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${TEST_NAME}.bc
        COMMAND clang -c ${INITIAL_CC_FLAGS} -emit-llvm ${TEST_SRC} -o ${CMAKE_CURRENT_BINARY_DIR}/${TEST_NAME}.bc
        DEPENDS ${TEST_SRC}
    )
    ADD_CUSTOM_TARGET(
        ${TEST_NAME}.m2r.bc
        COMMAND opt -passes=mem2reg -disable-verify ${CMAKE_CURRENT_BINARY_DIR}/${TEST_NAME}.bc -o ${CMAKE_CURRENT_BINARY_DIR}/${TEST_NAME}.m2r.bc
        DEPENDS ${TEST_NAME}.bc
    )
    ADD_CUSTOM_TARGET(
        ${TEST_NAME}.lvi.bc
        COMMAND opt -load-pass-plugin=${WDSYMYS_PLUGIN_PATH} -passes=wdsymys-lvi ${CMAKE_CURRENT_BINARY_DIR}/${TEST_NAME}.m2r.bc -o ${CMAKE_CURRENT_BINARY_DIR}/${TEST_NAME}.lvi.bc
        DEPENDS ${TEST_NAME}.m2r.bc WDSYMYS
    )
    ADD_CUSTOM_TARGET(
        ${TEST_NAME}.packed.bc
        COMMAND opt -load-pass-plugin=${WDSYMYS_PLUGIN_PATH} -passes=wdsymys-packing ${CMAKE_CURRENT_BINARY_DIR}/${TEST_NAME}.lvi.bc -o ${CMAKE_CURRENT_BINARY_DIR}/${TEST_NAME}.packed.bc
        DEPENDS ${TEST_NAME}.lvi.bc WDSYMYS
    )
    ADD_CUSTOM_TARGET(
        ${TEST_NAME}.m2r.ll
        COMMAND llvm-dis ${TEST_NAME}.m2r.bc -o ${CMAKE_CURRENT_BINARY_DIR}/${TEST_NAME}.m2r.ll
        DEPENDS ${TEST_NAME}.m2r.bc
    )
    ADD_CUSTOM_TARGET(
        ${TEST_NAME}.lvi.ll
        COMMAND llvm-dis ${TEST_NAME}.lvi.bc -o ${CMAKE_CURRENT_BINARY_DIR}/${TEST_NAME}.lvi.ll
        DEPENDS ${TEST_NAME}.lvi.bc
    )
    ADD_CUSTOM_TARGET(
        ${TEST_NAME}.packed.ll
        COMMAND llvm-dis ${TEST_NAME}.packed.bc -o ${CMAKE_CURRENT_BINARY_DIR}/${TEST_NAME}.packed.ll
        DEPENDS ${TEST_NAME}.packed.bc
    )
    ADD_CUSTOM_TARGET(
        ${TEST_NAME}.baseline
        COMMAND clang++ ${CMAKE_CURRENT_BINARY_DIR}/${TEST_NAME}.m2r.bc ${TESTCASE_CC_FLAGS} -o ${CMAKE_CURRENT_BINARY_DIR}/${TEST_NAME}.baseline -fstack-usage -Xclang -stack-usage-file -Xclang ${TEST_NAME}.baseline.su && (objdump -d ${TEST_NAME}.baseline > ${TEST_NAME}.baseline.s)
        DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/${TEST_NAME}.m2r.bc 
    )
    ADD_CUSTOM_TARGET(
        ${TEST_NAME}
        COMMAND clang++ ${CMAKE_CURRENT_BINARY_DIR}/${TEST_NAME}.packed.bc ${TESTCASE_CC_FLAGS} -o ${CMAKE_CURRENT_BINARY_DIR}/${TEST_NAME} -fstack-usage && (objdump -d ${TEST_NAME} > ${TEST_NAME}.s)
        DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/${TEST_NAME}.baseline ${CMAKE_CURRENT_BINARY_DIR}/${TEST_NAME}.packed.bc ${CMAKE_CURRENT_BINARY_DIR}/${TEST_NAME}.m2r.ll ${CMAKE_CURRENT_BINARY_DIR}/${TEST_NAME}.lvi.ll ${CMAKE_CURRENT_BINARY_DIR}/${TEST_NAME}.packed.ll
    )

    list(APPEND TESTCASES ${TEST_NAME})
endforeach()  

message(STATUS "TESTCASES: ${TESTCASES}")

ADD_CUSTOM_COMMAND(
    OUTPUT testcases
    DEPENDS ${TESTCASES}
)