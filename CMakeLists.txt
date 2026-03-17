cmake_minimum_required(VERSION 3.20)
project(RefactorTool VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

#===============================================================================
# 0. GET CLANG INSTALLATION DIR
#===============================================================================

find_package(Clang REQUIRED CONFIG)
find_package(LLVM REQUIRED CONFIG)
find_package(GTest REQUIRED)

set(CT_Clang_INSTALL_DIR "${Clang_DIR}/../../..")
set(CT_LLVM_INCLUDE_DIR "${LLVM_INCLUDE_DIRS}")

include_directories(${LLVM_INCLUDE_DIRS} ${CLANG_INCLUDE_DIRS})
link_directories(${LLVM_LIBRARY_DIRS})
add_definitions(${LLVM_DEFINITIONS})

#===============================================================================
# 1. VERIFY CLANG INSTALLATION DIR
#===============================================================================
set(CT_LLVM_INCLUDE_DIR "${CT_Clang_INSTALL_DIR}/include/llvm")
if(NOT EXISTS "${CT_LLVM_INCLUDE_DIR}")
    message(FATAL_ERROR
        " CT_Clang_INSTALL_DIR (${CT_LLVM_INCLUDE_DIR}) is invalid.")
endif()

set(CT_LLVM_CMAKE_FILE
    "${CT_Clang_INSTALL_DIR}/lib/cmake/clang/ClangConfig.cmake")
if(NOT EXISTS "${CT_LLVM_CMAKE_FILE}")
message(FATAL_ERROR
    " CT_LLVM_CMAKE_FILE (${CT_LLVM_CMAKE_FILE}) is invalid.")
endif()

#===============================================================================
# 2. LOAD CLANG CONFIGURATION
#    Extracted from:
#    http://llvm.org/docs/CMake.html#embedding-llvm-in-your-project
#===============================================================================
list(APPEND CMAKE_PREFIX_PATH "${CT_Clang_INSTALL_DIR}/lib/cmake/clang/")

find_package(Clang REQUIRED CONFIG)

# Sanity check. As Clang does not expose e.g. `CLANG_VERSION_MAJOR` through
# AddClang.cmake, we have to use LLVM_VERSION_MAJOR instead.
# TODO: Revisit when next version is released.
if("${LLVM_VERSION_MAJOR}" VERSION_LESS 19)
    message(FATAL_ERROR "Found LLVM ${LLVM_VERSION_MAJOR}, but need LLVM 19 or above")
endif()

message(STATUS "Found Clang ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using ClangConfig.cmake in: ${CT_Clang_INSTALL_DIR}")

message("CLANG STATUS:
    Includes (clang)    ${CLANG_INCLUDE_DIRS}
    Includes (llvm)     ${LLVM_INCLUDE_DIRS}"
)

# Set the LLVM and Clang header and library paths
include_directories(SYSTEM "${LLVM_INCLUDE_DIRS};${CLANG_INCLUDE_DIRS}")

#===============================================================================
# 3. CLANG-TUTOR BUILD CONFIGURATION
#===============================================================================
# Build type
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug CACHE
      STRING "Build type (default Debug):" FORCE)
endif()

# Compiler flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall\
    -fdiagnostics-color=always")

# LLVM/Clang is normally built without RTTI. Be consistent with that.
if(NOT LLVM_ENABLE_RTTI)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti")
endif()

# -fvisibility-inlines-hidden is set when building LLVM and on Darwin warnings
# are triggered if llvm-tutor is built without this flag (though otherwise it
# builds fine). For consistency, add it here too.
include(CheckCXXCompilerFlag)
check_cxx_compiler_flag("-fvisibility-inlines-hidden"
  SUPPORTS_FVISIBILITY_INLINES_HIDDEN_FLAG)
if(${SUPPORTS_FVISIBILITY_INLINES_HIDDEN_FLAG} EQUAL "1")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fvisibility-inlines-hidden")
endif()

# Set the build directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}")

# Pass Clang's location to the code
set(CLANG_RES_DIR "${CT_Clang_INSTALL_DIR}/lib/clang/${LLVM_VERSION_MAJOR}")

add_compile_definitions(
    CLANG_RESOURCE_DIR=${CLANG_RES_DIR}
    CLANG_INCLUDE_DIR=${CT_Clang_INSTALL_DIR}"/include/c++/v1"
)

#===============================================================================
# 4. ADD SUB-TARGETS
# Doing this at the end so that all definitions and link/include paths are
# available for the sub-projects.
#===============================================================================
add_subdirectory(src)

#===============================================================================
# Тесты
#===============================================================================

# Рекурсивно ищем все используемые в тестах .cpp файлы
file(GLOB TEST_SRC_FILES 
    "${CMAKE_SOURCE_DIR}/tests/*.cpp"
    "${CMAKE_SOURCE_DIR}/src/refactor_tool.cpp"
)

add_executable(refactor_tool_tests "${TEST_SRC_FILES}")

execute_process(COMMAND ${CMAKE_CXX_COMPILER} -print-resource-dir
    OUTPUT_VARIABLE CLANG_RESOURCE_DIR
    OUTPUT_STRIP_TRAILING_WHITESPACE)

target_include_directories(refactor_tool_tests PRIVATE
    "${CMAKE_SOURCE_DIR}/include"
)

target_link_libraries(refactor_tool_tests PRIVATE 
    GTest::GTest GTest::Main 
    clangTooling clangBasic clangASTMatchers
    LLVMSupport
)

# Включаем тестирование
enable_testing()
add_test(NAME Tests COMMAND refactor_tool_tests)