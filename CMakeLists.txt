# Google Test download & compile:
# (adapted from https://github.com/google/googletest/tree/master/googletest)
# Download and unpack googletest at configure time
configure_file(cmake/GTest.txt.in googletest-download/CMakeLists.txt)
execute_process(COMMAND ${CMAKE_COMMAND} -G "${CMAKE_GENERATOR}" .
                RESULT_VARIABLE result
                WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/googletest-download )
if(result)
  message(FATAL_ERROR "CMake step for googletest failed: ${result}")
endif()
execute_process(COMMAND ${CMAKE_COMMAND} --build .
                RESULT_VARIABLE result
                WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/googletest-download )
if(result)
  message(FATAL_ERROR "Build step for googletest failed: ${result}")
endif()

# Prevent overriding the parent project's compiler/linker
# settings on Windows
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

# Add googletest directly to our build. This defines
# the gtest and gtest_main targets.
add_subdirectory(${CMAKE_BINARY_DIR}/googletest-src
                 ${CMAKE_BINARY_DIR}/googletest-build
                 EXCLUDE_FROM_ALL)

# The gtest/gtest_main targets carry header search path
# dependencies automatically when using CMake 2.8.11 or
# later. Otherwise we have to add them here ourselves.
if (CMAKE_VERSION VERSION_LESS 2.8.11)
  include_directories("${gtest_SOURCE_DIR}/include")
endif()


# PEGTL download & compile:
# (adapted from https://github.com/google/googletest/tree/master/googletest)
# Download and unpack googletest at configure time
configure_file(cmake/PEGTL.txt.in pegtl-download/CMakeLists.txt)
execute_process(COMMAND ${CMAKE_COMMAND} -G "${CMAKE_GENERATOR}" .
                RESULT_VARIABLE result
                WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/pegtl-download )
if(result)
  message(FATAL_ERROR "CMake step for PEGTL failed: ${result}")
endif()
execute_process(COMMAND ${CMAKE_COMMAND} --build .
                RESULT_VARIABLE result
                WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/pegtl-download )
if(result)
  message(FATAL_ERROR "Build step for PEGTL failed: ${result}")
endif()

# Add PEGTL directly to our build.
add_subdirectory(${CMAKE_BINARY_DIR}/pegtl-src
                 ${CMAKE_BINARY_DIR}/pegtl-build
                 EXCLUDE_FROM_ALL)

# Same as above
if (CMAKE_VERSION VERSION_LESS 2.8.11)
  include_directories("${pegtl_SOURCE_DIR}/include")
endif()


cmake_minimum_required(VERSION 3.9)
project(Seq)

find_package(LLVM REQUIRED CONFIG)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -pedantic -fstrict-aliasing -Wall -Wno-return-type-c-linkage")
include_directories(src/)
include_directories(include/)


# Seq runtime library
add_library(seqrt SHARED include/seq/lib.h
                         src/seq/lib.cpp)
target_link_libraries(seqrt gc)


# Seq compiler library
include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})
add_library(seq SHARED include/seq/any.h
                       include/seq/array.h
                       include/seq/base.h
                       include/seq/common.h
                       include/seq/expr.h
                       include/seq/func.h
                       include/seq/funct.h
                       include/seq/generic.h
                       include/seq/lang.h
                       include/seq/llvm.h
                       include/seq/num.h
                       include/seq/ops.h
                       include/seq/optional.h
                       include/seq/parser.h
                       include/seq/patterns.h
                       include/seq/record.h
                       include/seq/ref.h
                       include/seq/seq.h
                       include/seq/seqt.h
                       include/seq/source.h
                       include/seq/stmt.h
                       include/seq/types.h
                       include/seq/void.h
                       include/seq/var.h
                       src/lang/expr.cpp
                       src/lang/func.cpp
                       src/lang/lang.cpp
                       src/lang/ops.cpp
                       src/lang/patterns.cpp
                       src/lang/seq.cpp
                       src/lang/stmt.cpp
                       src/lang/var.cpp
                       src/parser/grammar.h
                       src/parser/parser.cpp
                       src/types/any.cpp
                       src/types/array.cpp
                       src/types/base.cpp
                       src/types/funct.cpp
                       src/types/generic.cpp
                       src/types/num.cpp
                       src/types/optional.cpp
                       src/types/record.cpp
                       src/types/ref.cpp
                       src/types/seqt.cpp
                       src/types/source.cpp
                       src/types/types.cpp
                       src/types/void.cpp
                       src/util/ocaml.cpp)
llvm_map_components_to_libnames(LLVM_LIBS support core passes irreader x86asmparser x86info x86codegen mcjit orcjit ipo coroutines)
target_link_libraries(seq ${LLVM_LIBS} seqrt pegtl)


# Seq command-line tool
add_executable(seqc src/seq/main.cpp)
target_link_libraries(seqc seq)
set_target_properties(seqc PROPERTIES OUTPUT_NAME seq)


# Seq test
add_executable(seqtest test/main.cpp
                       test/core/standalonetest.h
                       test/testhelp.h)
target_compile_definitions(seqtest PRIVATE TEST_DIR="${CMAKE_CURRENT_SOURCE_DIR}/test")
target_link_libraries(seqtest seq gtest_main)
