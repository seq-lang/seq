cmake_minimum_required(VERSION 3.9)
project(Seq)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -pedantic -Wall -Wno-return-type-c-linkage")
include_directories(src/)
include_directories(include/)

find_package(LLVM REQUIRED CONFIG)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")


# Seq runtime library
add_library(seqrt SHARED include/seq/lib.h
                         src/seq/lib.cpp
                         src/seq/exc.cpp)
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
                       include/seq/ptr.h
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
                       src/types/any.cpp
                       src/types/array.cpp
                       src/types/base.cpp
                       src/types/funct.cpp
                       src/types/generic.cpp
                       src/types/num.cpp
                       src/types/optional.cpp
                       src/types/ptr.cpp
                       src/types/record.cpp
                       src/types/ref.cpp
                       src/types/seqt.cpp
                       src/types/source.cpp
                       src/types/types.cpp
                       src/types/void.cpp
                       src/util/ocaml.cpp)
llvm_map_components_to_libnames(LLVM_LIBS support core passes irreader x86asmparser x86info x86codegen mcjit orcjit ipo coroutines)
target_link_libraries(seq ${LLVM_LIBS} seqrt)


# Seq parsing library
find_program(OCAMLFIND NAMES ocamlfind)
execute_process(COMMAND ${OCAMLFIND} ocamlc -where
                RESULT_VARIABLE result
                OUTPUT_VARIABLE OCAML_STDLIB_PATH
                OUTPUT_STRIP_TRAILING_WHITESPACE)
if (result)
  message(FATAL_ERROR "CMake step for ocamlfind failed: ${result}")
endif()

message(STATUS "Found OCaml: ${OCAML_STDLIB_PATH}")
include_directories(${OCAML_STDLIB_PATH})
link_directories(${OCAML_STDLIB_PATH})

if(APPLE)
  set(DUNE_PROFILE "macos-dev")
else()
  set(DUNE_PROFILE "dev")
endif()

set(DUNE_BUILD_DIR "${CMAKE_CURRENT_BINARY_DIR}/dune")
set(LIB_SEQPARSE "${CMAKE_CURRENT_BINARY_DIR}/libseqparser.so")

find_program(OPAM NAMES opam)
if(NOT OPAM)
  message(FATAL_ERROR "opam command not found")
endif()

add_custom_command(OUTPUT ${LIB_SEQPARSE}
                   COMMAND ${OPAM} exec -- dune build --profile=${DUNE_PROFILE} --build-dir=${DUNE_BUILD_DIR} src/main.exe src/main.so
                   COMMAND cp ${DUNE_BUILD_DIR}/default/src/main.so ${LIB_SEQPARSE}
                   WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/ocaml"
                   DEPENDS ocaml/src/dune
                           ocaml/src/main.ml
                           ocaml/dune-project
                   COMMENT "Compiling Seq's OCaml components"
                   VERBATIM)
add_custom_target(seqparse_target DEPENDS ${LIB_SEQPARSE})
add_dependencies(seqparse_target seq)
add_library(seqparse SHARED IMPORTED)
add_dependencies(seqparse seqparse_target)
set_target_properties(seqparse PROPERTIES IMPORTED_LOCATION ${LIB_SEQPARSE})


# Seq command-line tool
add_executable(seqc src/seq/main.cpp)
target_link_libraries(seqc seq seqparse asmrun)
set_target_properties(seqc PROPERTIES OUTPUT_NAME seq)


# Seq test
add_executable(seqtest test/main.cpp)
target_compile_definitions(seqtest PRIVATE TEST_DIR="${CMAKE_CURRENT_SOURCE_DIR}/test")
target_link_libraries(seqtest seq seqparse asmrun)
