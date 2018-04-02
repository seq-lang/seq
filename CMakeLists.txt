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
add_library(seq SHARED include/any.h
                       include/array.h
                       include/base.h
                       include/basestage.h
                       include/call.h
                       include/chunk.h
                       include/collect.h
                       include/common.h
                       include/copy.h
                       include/count.h
                       include/exc.h
                       include/filter.h
                       include/foreach.h
                       include/func.h
                       include/hash.h
                       include/io.h
                       include/lambda.h
                       include/len.h
                       include/mem.h
                       include/num.h
                       include/op.h
                       include/pipeline.h
                       include/print.h
                       include/range.h
                       include/range.h
                       include/revcomp.h
                       include/seq.h
                       include/seq.h
                       include/seqt.h
                       include/serialize.h
                       include/split.h
                       include/stage.h
                       include/stageutil.h
                       include/substr.h
                       include/types.h
                       include/util.h
                       include/var.h
                       include/void.h
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
