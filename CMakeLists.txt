cmake_minimum_required(VERSION 3.10)
project(Seq)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fvisibility-inlines-hidden -pedantic -Wall -Wno-return-type-c-linkage")
set(CMAKE_CXX_FLAGS_DEBUG "-g")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")
include_directories(compiler/include)
include_directories(runtime)

find_package(LLVM REQUIRED CONFIG)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)


# Seq runtime library
option(SEQ_THREADED "compile runtime library for multithreading" OFF)
option(SEQ_JITBRIDGE "support JIT interoperability" OFF)
find_package(ZLIB REQUIRED)

find_library(HTS_LIB NAMES libhts.a libhts)
if(NOT HTS_LIB)
  message(FATAL_ERROR "HTSlib not found")
else()
  message(STATUS "Found HTSlib: ${HTS_LIB}")
endif()

find_library(GC_LIB NAMES libgc.a libgc)
if(NOT GC_LIB)
  message(FATAL_ERROR "GC not found")
else()
  message(STATUS "Found GC: ${GC_LIB}")
endif()

add_library(seqrt SHARED runtime/lib.h
                         runtime/lib.cpp
                         runtime/exc.cpp
                         runtime/ksw2/ksw2.h
                         runtime/ksw2/ksw2_extd2_sse.cpp
                         runtime/ksw2/ksw2_exts2_sse.cpp
                         runtime/ksw2/ksw2_extz2_sse.cpp
                         runtime/ksw2/ksw2_gg2_sse.cpp)
target_link_libraries(seqrt PUBLIC bz2 lzma ${ZLIB_LIBRARIES} ${GC_LIB} ${HTS_LIB} Threads::Threads)
if(NOT APPLE)

if(SEQ_THREADED)
  find_package(OpenMP REQUIRED)
  target_link_libraries(seqrt PUBLIC OpenMP::OpenMP_CXX)
  target_compile_definitions(seqrt PRIVATE THREADED=1)
else()
  target_compile_definitions(seqrt PRIVATE THREADED=0)
endif()


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
set(DUNE_OUTPUT_NAME "main.exe.o")

find_program(OPAM NAMES opam)
if(NOT OPAM)
  message(FATAL_ERROR "opam command not found")
endif()

add_custom_command(OUTPUT ${LIB_SEQPARSE} ${CMAKE_CURRENT_BINARY_DIR}/dummy
                   COMMAND ${OPAM} exec -- dune build --build-dir=${DUNE_BUILD_DIR} ${DUNE_OUTPUT_NAME}
                   COMMAND cp ${DUNE_BUILD_DIR}/default/${DUNE_OUTPUT_NAME} ${LIB_SEQPARSE}
                   WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/parser"
                   COMMENT "Compiling Seq's OCaml components"
                   VERBATIM)
add_custom_target(seqparse_target DEPENDS ${LIB_SEQPARSE})
add_dependencies(seqparse_target seq)
add_library(seqparse SHARED IMPORTED)
add_dependencies(seqparse seqparse_target)
set_target_properties(seqparse PROPERTIES IMPORTED_LOCATION ${LIB_SEQPARSE})
list(APPEND OCAML_STATIC "")
foreach(OCAML_STUB IN ITEMS
    ctypes/libctypes-foreign-threaded_stubs.a
    ctypes/libctypes-foreign-base_stubs.a
    ctypes/libctypes_stubs.a
    core/libcore_stubs.a
    core_kernel/libcore_kernel_stubs.a
    spawn/libspawn_stubs.a
    time_now/libtime_now_stubs.a
    ppx_expect/collector/libexpect_test_collector_stubs.a
    bin_prot/libbin_prot_stubs.a
    base_bigstring/libbase_bigstring_stubs.a
    base/libbase_stubs.a
    integers/libintegers_stubs.a
    integers/libintegers.a
    ocaml/libcamlstr.a
    ocaml/libbigarray.a
    ocaml/libthreadsnat.a
    ocaml/libthreads.a
    ocaml/libunix.a)
  set(OCAML_STUB_PATH "${OCAML_STDLIB_PATH}/../${OCAML_STUB}")
  if(EXISTS ${OCAML_STUB_PATH})
    list(APPEND OCAML_STATIC ${OCAML_STUB_PATH})
  endif()
endforeach(OCAML_STUB)


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
                       compiler/include/seq/pipeline.h
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
                       compiler/lang/pipeline.cpp
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
                       compiler/util/ocaml.cpp
                       ${LIB_SEQPARSE})
llvm_map_components_to_libnames(LLVM_LIBS support core passes irreader x86asmparser x86info x86codegen mcjit orcjit ipo coroutines)
target_link_libraries(seq ${LLVM_LIBS} ${OCAML_STATIC} ffi seqrt)

if(SEQ_JITBRIDGE)
  add_library(seqjit SHARED compiler/util/jit.cpp)
  target_link_libraries(seqjit seq)
endif()


# Seq command-line tool
add_executable(seqc runtime/main.cpp)
target_link_libraries(seqc seq)


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

add_executable(seqtest test/main.cpp)
target_link_libraries(seqtest seq gtest_main)
target_compile_definitions(seqtest PRIVATE TEST_DIR="${CMAKE_CURRENT_SOURCE_DIR}/test")
