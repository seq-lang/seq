cmake_minimum_required(VERSION 3.12)
project(Seq)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -pedantic -Wall -Wno-return-type-c-linkage")
include_directories(compiler/include)
include_directories(runtime)

find_package(LLVM REQUIRED CONFIG)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")


# Seq runtime library
option(SEQ_THREADED "compile runtime library for multithreading" OFF)
option(SEQ_PYBRIDGE "support Python interoperability" ON)
find_package(ZLIB REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(GC REQUIRED bdw-gc)
add_library(seqrt SHARED runtime/lib.h
                         runtime/lib.cpp
                         runtime/exc.cpp
                         runtime/ksw2/ksw2.h
                         runtime/ksw2/ksw2_extd2_sse.cpp
                         runtime/ksw2/ksw2_exts2_sse.cpp
                         runtime/ksw2/ksw2_extz2_sse.cpp
                         runtime/ksw2/ksw2_gg2_sse.cpp
                         runtime/pybridge.cpp)
target_link_libraries(seqrt PUBLIC ${ZLIB_LIBRARIES} ${GC_LINK_LIBRARIES} hts)
target_include_directories(seqrt PRIVATE ${GC_INCLUDE_DIRS})
target_compile_options(seqrt PRIVATE ${GC_CFLAGS_OTHER} -O3)

if(SEQ_THREADED)
  find_package(OpenMP REQUIRED)
  target_link_libraries(seqrt PUBLIC OpenMP::OpenMP_CXX)
  target_compile_definitions(seqrt PRIVATE THREADED=1)
else()
  target_compile_definitions(seqrt PRIVATE THREADED=0)
endif()

if(SEQ_PYBRIDGE)
  find_package(PythonInterp REQUIRED)
  find_package(PythonLibs REQUIRED)
  include_directories(${PYTHON_INCLUDE_DIRS})
  target_link_libraries(seqrt PUBLIC ${PYTHON_LIBRARIES})
  target_compile_definitions(seqrt PRIVATE PYBRIDGE=1)
else()
  target_compile_definitions(seqrt PRIVATE PYBRIDGE=0)
endif()


# Seq compiler library
include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})
add_library(seq SHARED compiler/include/seq/any.h
                       compiler/include/seq/array.h
                       compiler/include/seq/base.h
                       compiler/include/seq/common.h
                       compiler/include/seq/expr.h
                       compiler/include/seq/func.h
                       compiler/include/seq/funct.h
                       compiler/include/seq/generic.h
                       compiler/include/seq/lang.h
                       compiler/include/seq/llvm.h
                       compiler/include/seq/num.h
                       compiler/include/seq/ops.h
                       compiler/include/seq/optional.h
                       compiler/include/seq/parser.h
                       compiler/include/seq/patterns.h
                       compiler/include/seq/ptr.h
                       compiler/include/seq/record.h
                       compiler/include/seq/ref.h
                       compiler/include/seq/seq.h
                       compiler/include/seq/seqt.h
                       compiler/include/seq/stmt.h
                       compiler/include/seq/types.h
                       compiler/include/seq/void.h
                       compiler/include/seq/var.h
                       compiler/lang/expr.cpp
                       compiler/lang/func.cpp
                       compiler/lang/lang.cpp
                       compiler/lang/ops.cpp
                       compiler/lang/patterns.cpp
                       compiler/lang/seq.cpp
                       compiler/lang/stmt.cpp
                       compiler/lang/var.cpp
                       compiler/types/any.cpp
                       compiler/types/array.cpp
                       compiler/types/base.cpp
                       compiler/types/funct.cpp
                       compiler/types/generic.cpp
                       compiler/types/num.cpp
                       compiler/types/optional.cpp
                       compiler/types/ptr.cpp
                       compiler/types/record.cpp
                       compiler/types/ref.cpp
                       compiler/types/seqt.cpp
                       compiler/types/types.cpp
                       compiler/types/void.cpp
                       compiler/util/ocaml.cpp)
llvm_map_components_to_libnames(LLVM_LIBS support core passes irreader x86asmparser x86info x86codegen mcjit orcjit ipo coroutines)
target_link_libraries(seq ${LLVM_LIBS} seqrt)


# Seq parsing library
find_program(OCAMLFIND NAMES ocamlfind)
execute_process(COMMAND ${OCAMLFIND} ocamlc -where
                RESULT_VARIABLE result
                OUTPUT_VARIABLE OCAML_STDLIB_PATH
                OUTPUT_STRIP_TRAILING_WHITESPACE)
if(result)
  message(FATAL_ERROR "CMake step for ocamlfind failed: ${result}")
endif()

message(STATUS "Found OCaml: ${OCAML_STDLIB_PATH}")
include_directories(${OCAML_STDLIB_PATH})
link_directories(${OCAML_STDLIB_PATH})

set(DUNE_BUILD_DIR "${CMAKE_CURRENT_BINARY_DIR}/dune")
set(LIB_SEQPARSE "${CMAKE_CURRENT_BINARY_DIR}/seqparser.o")

find_program(OPAM NAMES opam)
if(NOT OPAM)
  message(FATAL_ERROR "opam command not found")
endif()

# Hack to run dune rebuild every time
add_custom_target(
  always_runs ALL
  DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/dummy
)
# OCaml does not look for system libraries; these have to be explicitly passed on
if(APPLE)
  set(DUNE_LIBRARY_PATH "/usr/local/lib")
else()
  set(DUNE_LIBRARY_PATH "/usr/lib/x86_64-linux-gnu")
endif()
add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/dummy ${LIB_SEQPARSE}
                   COMMAND DUNE_LIBRARY_PATH=${DUNE_LIBRARY_PATH} ${OPAM} exec -- dune build --profile=dev --build-dir=${DUNE_BUILD_DIR} main.exe.o
                   COMMAND cp ${DUNE_BUILD_DIR}/default/main.exe.o ${LIB_SEQPARSE}
                   WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/parser"
                   DEPENDS parser/dune
                           parser/main.ml
                           parser/dune-project
                   COMMENT "Compiling Seq's OCaml components"
                   VERBATIM)
add_custom_target(seqparse_target DEPENDS ${LIB_SEQPARSE})
add_dependencies(seqparse_target seq)
add_library(seqparse SHARED IMPORTED)
add_dependencies(seqparse seqparse_target)
set_target_properties(seqparse PROPERTIES IMPORTED_LOCATION ${LIB_SEQPARSE})


# Seq command-line tool
add_executable(seqc runtime/main.cpp ${LIB_SEQPARSE})
target_link_libraries(seqc seq ffi)
set_target_properties(seqc PROPERTIES OUTPUT_NAME seq)


# Seq test
# Download and unpack googletest at configure time
configure_file(test/CMakeLists.txt.in googletest-download/CMakeLists.txt)
execute_process(COMMAND ${CMAKE_COMMAND} -G "${CMAKE_GENERATOR}" .
                RESULT_VARIABLE result
                WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/googletest-download)
if(result)
  message(FATAL_ERROR "CMake step for googletest failed: ${result}")
endif()
execute_process(COMMAND ${CMAKE_COMMAND} --build .
                RESULT_VARIABLE result
                WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/googletest-download )
if(result)
  message(FATAL_ERROR "Build step for googletest failed: ${result}")
endif()

# Prevent overriding the parent project's compiler/linker
# settings on Windows
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

# Add googletest directly to our build. This defines
# the gtest and gtest_main targets.
add_subdirectory(${CMAKE_CURRENT_BINARY_DIR}/googletest-src
                 ${CMAKE_CURRENT_BINARY_DIR}/googletest-build
                 EXCLUDE_FROM_ALL)

add_executable(seqtest test/main.cpp ${LIB_SEQPARSE})
target_compile_definitions(seqtest PRIVATE TEST_DIR="${CMAKE_CURRENT_SOURCE_DIR}/test")
target_link_libraries(seqtest seq ffi gtest_main)
