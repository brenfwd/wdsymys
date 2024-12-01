add_compile_definitions(LOCAL=1)

file(GLOB SOURCES CONFIGURE_DEPENDS "*.cpp")
add_llvm_pass_plugin(WDSYMYS ${SOURCES})

#add_subdirectory(test)
#add_dependencies(WDSYMYS build_llvm)