add_compile_definitions(LOCAL=1)

add_llvm_pass_plugin(WDSYMYS pass.cpp)

#add_subdirectory(test)
#add_dependencies(WDSYMYS build_llvm)