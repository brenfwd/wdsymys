add_compile_definitions(LOCAL=1)

add_llvm_pass_plugin(WDSYMYS pass.cpp)
target_include_directories(WDSYMYS PUBLIC ../load-latency/)