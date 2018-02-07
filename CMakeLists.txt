cmake_minimum_required(VERSION 3.9)
project(Seq)

# Seq library
find_package(LLVM REQUIRED CONFIG)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pedantic -pedantic-errors -fstrict-aliasing")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")
include_directories(include/)

include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})
add_library(seq SHARED src/seq.cpp include/seq.h src/util.cpp include/util.h src/io.cpp include/io.h src/stages/stage.cpp include/stage.h src/stages/op.cpp include/op.h src/stages/revcomp.cpp include/revcomp.h src/stages/print.cpp include/print.h src/stages/substr.cpp include/substr.h src/stages/split.cpp include/split.h src/stages/filter.cpp include/filter.h src/stages/copy.cpp include/copy.h src/exc.cpp include/exc.h include/types.h src/pipeline.cpp include/pipeline.h src/var.cpp include/var.h src/stages/basestage.cpp include/basestage.h include/seq.h src/stages/stageutil.cpp include/stageutil.h)
llvm_map_components_to_libnames(llvm_libs support core irreader x86asmparser x86info x86codegen mcjit orcjit)
target_link_libraries(seq ${llvm_libs})

# Seq test
add_executable(seqtest test/test.cpp)
target_link_libraries(seqtest seq)
