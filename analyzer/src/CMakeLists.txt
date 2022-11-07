cmake_minimum_required(VERSION 3.5.1)
project(KANALYZER)

find_package(LLVM REQUIRED CONFIG)
find_package(ZLIB)
find_package(Z3)
message(STATUS "Z3_FOUND: ${Z3_FOUND}")
message(STATUS "Found Z3 ${Z3_VERSION_STRING}")
message(STATUS "Z3_DIR: ${Z3_DIR}")
message(STATUS "Z3_LIBRARIES: ${Z3_LIBRARIES}")

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

# Set your project compile flags.
# E.g. if using the C++ header files
# you will need to enable C++14 support
# for your compiler.
# Check for C++14 support and set the compilation flag
include(CheckCXXCompilerFlag)
#CHECK_CXX_COMPILER_FLAG("-std=c++14" COMPILER_SUPPORTS_CXX14)
# if(COMPILER_SUPPORTS_CXX14)
# 	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14 -fno-rtti -fPIC -Wall")
# else()
# 	message(STATUS "The compiler ${CMAKE_CXX_COMPILER} has no C++14 support. Please use a different C++ compiler.")
# endif()

include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

#include_directories(${Z3_DIR}/include)
add_subdirectory (lib)
