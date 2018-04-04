cmake_minimum_required(VERSION 3.9)
project(Seq)

# Seq library
find_package(LLVM REQUIRED CONFIG)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pedantic -pedantic-errors -fstrict-aliasing")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")
include_directories(src/)
include_directories(include/)

include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})
add_library(seq SHARED include/seq/any.h
                       include/seq/array.h
                       include/seq/base.h
                       include/seq/basestage.h
                       include/seq/call.h
                       include/seq/chunk.h
                       include/seq/collect.h
                       include/seq/common.h
                       include/seq/copy.h
                       include/seq/count.h
                       include/seq/exc.h
                       include/seq/filter.h
                       include/seq/foreach.h
                       include/seq/func.h
                       include/seq/hash.h
                       include/seq/io.h
                       include/seq/lambda.h
                       include/seq/len.h
                       include/seq/mem.h
                       include/seq/num.h
                       include/seq/op.h
                       include/seq/pipeline.h
                       include/seq/print.h
                       include/seq/range.h
                       include/seq/revcomp.h
                       include/seq/seq.h
                       include/seq/seqt.h
                       include/seq/serialize.h
                       include/seq/split.h
                       include/seq/stage.h
                       include/seq/stageutil.h
                       include/seq/substr.h
                       include/seq/types.h
                       include/seq/util.h
                       include/seq/var.h
                       include/seq/void.h
                       src/exc.cpp
                       src/func.cpp
                       src/io.cpp
                       src/llvm.h
                       src/pipeline.cpp
                       src/seq.cpp
                       src/seqdata.h
                       src/stages/basestage.cpp
                       src/stages/call.cpp
                       src/stages/chunk.cpp
                       src/stages/collect.cpp
                       src/stages/copy.cpp
                       src/stages/count.cpp
                       src/stages/filter.cpp
                       src/stages/foreach.cpp
                       src/stages/hash.cpp
                       src/stages/lambda.cpp
                       src/stages/len.cpp
                       src/stages/mem.cpp
                       src/stages/op.cpp
                       src/stages/print.cpp
                       src/stages/range.cpp
                       src/stages/revcomp.cpp
                       src/stages/serialize.cpp
                       src/stages/split.cpp
                       src/stages/stage.cpp
                       src/stages/stageutil.cpp
                       src/stages/substr.cpp
                       src/types/any.cpp
                       src/types/array.cpp
                       src/types/base.cpp
                       src/types/num.cpp
                       src/types/seqt.cpp
                       src/types/types.cpp
                       src/types/void.cpp
                       src/util.cpp
                       src/var.cpp)
llvm_map_components_to_libnames(llvm_libs support core irreader x86asmparser x86info x86codegen mcjit orcjit)
target_link_libraries(seq ${llvm_libs})

# Seq test
add_executable(seqtest test/test.cpp)
target_link_libraries(seqtest seq)
