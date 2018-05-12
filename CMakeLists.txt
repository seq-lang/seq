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

# Seq library
cmake_minimum_required(VERSION 3.9)
project(Seq)

find_package(LLVM REQUIRED CONFIG)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pedantic -pedantic-errors -fstrict-aliasing")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall \
                                        -Wno-gnu-zero-variadic-macro-arguments \
                                        -Wno-return-type-c-linkage")
include_directories(src/)
include_directories(include/)

include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})
add_library(seq SHARED include/seq/any.h
                       include/seq/array.h
                       include/seq/arrayexpr.h
                       include/seq/base.h
                       include/seq/basestage.h
                       include/seq/call.h
                       include/seq/capture.h
                       include/seq/cell.h
                       include/seq/chunk.h
                       include/seq/collect.h
                       include/seq/common.h
                       include/seq/copy.h
                       include/seq/count.h
                       include/seq/exc.h
                       include/seq/expr.h
                       include/seq/exprstage.h
                       include/seq/filter.h
                       include/seq/foreach.h
                       include/seq/func.h
                       include/seq/getelemexpr.h
                       include/seq/getitem.h
                       include/seq/hash.h
                       include/seq/io.h
                       include/seq/lambda.h
                       include/seq/len.h
                       include/seq/lookupexpr.h
                       include/seq/makerec.h
                       include/seq/mem.h
                       include/seq/num.h
                       include/seq/numexpr.h
                       include/seq/op.h
                       include/seq/parser.h
                       include/seq/pipeline.h
                       include/seq/print.h
                       include/seq/range.h
                       include/seq/record.h
                       include/seq/recordexpr.h
                       include/seq/revcomp.h
                       include/seq/seq.h
                       include/seq/seqt.h
                       include/seq/serialize.h
                       include/seq/source.h
                       include/seq/split.h
                       include/seq/stage.h
                       include/seq/stageutil.h
                       include/seq/strexpr.h
                       include/seq/substr.h
                       include/seq/types.h
                       include/seq/util.h
                       include/seq/var.h
                       include/seq/varexpr.h
                       include/seq/void.h
                       src/expr/arrayexpr.cpp
                       src/expr/expr.cpp
                       src/expr/getelemexpr.cpp
                       src/expr/lookupexpr.cpp
                       src/expr/numexpr.cpp
                       src/expr/recordexpr.cpp
                       src/expr/strexpr.cpp
                       src/expr/varexpr.cpp
                       src/parser/grammar.h
                       src/parser/parser.cpp
                       src/stages/basestage.cpp
                       src/stages/call.cpp
                       src/stages/capture.cpp
                       src/stages/chunk.cpp
                       src/stages/collect.cpp
                       src/stages/copy.cpp
                       src/stages/count.cpp
                       src/stages/exprstage.cpp
                       src/stages/filter.cpp
                       src/stages/foreach.cpp
                       src/stages/getitem.cpp
                       src/stages/hash.cpp
                       src/stages/lambda.cpp
                       src/stages/len.cpp
                       src/stages/makerec.cpp
                       src/stages/mem.cpp
                       src/stages/op.cpp
                       src/stages/print.cpp
                       src/stages/range.cpp
                       src/stages/revcomp.cpp
                       src/stages/serialize.cpp
                       src/stages/source.cpp
                       src/stages/split.cpp
                       src/stages/stage.cpp
                       src/stages/stageutil.cpp
                       src/stages/substr.cpp
                       src/types/any.cpp
                       src/types/array.cpp
                       src/types/base.cpp
                       src/types/num.cpp
                       src/types/record.cpp
                       src/types/seqt.cpp
                       src/types/types.cpp
                       src/types/void.cpp
                       src/cell.cpp
                       src/exc.cpp
                       src/func.cpp
                       src/io.cpp
                       src/llvm.h
                       src/pipeline.cpp
                       src/seq.cpp
                       src/seqdata.h
                       src/util.cpp
                       src/var.cpp)
llvm_map_components_to_libnames(LLVM_LIBS support core irreader x86asmparser x86info x86codegen mcjit orcjit)
target_link_libraries(seq ${LLVM_LIBS} pegtl)

# Seq test
add_executable(seqtest test/main.cpp
                       test/core/bigtest.h
                       test/core/branchtest.h
                       test/core/standalonetest.h
                       test/stages/calltest.h
                       test/stages/chunktest.h
                       test/stages/collecttest.h
                       test/stages/copytest.h
                       test/stages/counttest.h
                       test/stages/filtertest.h
                       test/stages/lentest.h
                       test/stages/makerectest.h
                       test/stages/memtest.h
                       test/stages/serializetest.h
                       test/testhelp.h)
target_compile_definitions(seqtest PRIVATE TEST_DIR="${CMAKE_CURRENT_SOURCE_DIR}/test")
target_link_libraries(seqtest seq gtest_main)
