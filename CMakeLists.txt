cmake_minimum_required(VERSION 3.9)
project(Seq)

# Seq library
set(LLVM_DIR /usr/local/Cellar/llvm/5.0.1/lib/cmake/llvm/)
find_package(LLVM REQUIRED CONFIG)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pedantic -pedantic-errors -fstrict-aliasing")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")

include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})
add_library(seq SHARED src/seq.cpp src/seq.h src/util.cpp src/util.h src/io.cpp src/io.h src/stages/stage.cpp src/stages/stage.h src/stages/op.cpp src/stages/op.h src/stages/revcomp.cpp src/stages/revcomp.h src/stages/print.cpp src/stages/print.h src/stages/substr.cpp src/stages/substr.h src/stages/split.cpp src/stages/split.h src/stages/filter.cpp src/stages/filter.h src/stages/copy.cpp src/stages/copy.h src/exc.cpp src/exc.h src/types.h)
llvm_map_components_to_libnames(llvm_libs support core irreader x86asmparser x86info x86codegen mcjit orcjit)
target_link_libraries(seq ${llvm_libs})

# Seq test
add_executable(seqtest test/test.cpp)
target_link_libraries(seqtest seq)
