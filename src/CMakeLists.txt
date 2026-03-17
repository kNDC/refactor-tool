add_executable(refactor_tool main.cpp refactor_tool.cpp)

target_compile_options(refactor_tool PRIVATE 
    -stdlib=libc++ -fexceptions)
target_link_options(refactor_tool PRIVATE 
    -stdlib=libc++)

add_compile_definitions(
    REFACTOR_DEBUG=$<CONFIG:Debug>
)

target_include_directories(refactor_tool PRIVATE
    "${CMAKE_SOURCE_DIR}/include"
)

target_link_libraries(refactor_tool PRIVATE 
    clangTooling clangBasic clangASTMatchers
)