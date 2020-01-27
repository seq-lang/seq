cmake_minimum_required(VERSION 3.10)
project(Seq)

set(CMAKE_CXX_STANDARD 14)
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

if(SEQ_THREADED)
  find_package(OpenMP REQUIRED)
  target_link_libraries(seqrt PUBLIC OpenMP::OpenMP_CXX)
  target_compile_definitions(seqrt PRIVATE THREADED=1)
else()
  target_compile_definitions(seqrt PRIVATE THREADED=0)
endif()


# Seq parsing library
execute_process(COMMAND ocamlc -where
                RESULT_VARIABLE result
                OUTPUT_VARIABLE OCAML_STDLIB_PATH
                OUTPUT_STRIP_TRAILING_WHITESPACE)
if(result)
  message(FATAL_ERROR "CMake step for ocaml failed: ${result}")
endif()

message(STATUS "Found OCaml: ${OCAML_STDLIB_PATH}")
include_directories(${OCAML_STDLIB_PATH})
link_directories(${OCAML_STDLIB_PATH})

# Menhir grammar generation: menhir grammar.mly --base grammar && rm grammar.mli
set(LIB_SEQPARSE "${CMAKE_CURRENT_BINARY_DIR}/ocaml/seqparser.o")
add_custom_command(OUTPUT ${LIB_SEQPARSE} ${CMAKE_CURRENT_BINARY_DIR}/dummy
                   COMMAND cp -r ${CMAKE_SOURCE_DIR}/compiler/parser/ocaml .
                   COMMAND cd ocaml && ocamllex -q -o lexer.ml lexer.mll
                   COMMAND cd ocaml && ocamlopt 
                    -strict-sequence -strict-formats -short-paths -keep-locs -g -output-complete-obj 
                    -o ${LIB_SEQPARSE}
                    ast.ml grammar.ml lexer.ml main.ml
                   WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
                   COMMENT "Compiling Seq's OCaml components"
                   VERBATIM)
add_custom_target(seqparse_target DEPENDS ${LIB_SEQPARSE})
add_dependencies(seqparse_target seq)
add_library(seqparse SHARED IMPORTED)
add_dependencies(seqparse seqparse_target)
set_target_properties(seqparse PROPERTIES IMPORTED_LOCATION ${LIB_SEQPARSE})

# Seq compiler library
include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

file(GLOB SEQ_HFILES compiler/include/fmt/*.h
                     compiler/include/parser/*.h
                     compiler/include/parser/ast/*.h
                     compiler/include/parser/ast/transform/*.h
                     compiler/include/parser/ast/codegen/*.h
                     compiler/include/seq/*.h)
file(GLOB SEQ_CPPFILES compiler/lang/*.cpp
                       compiler/types/*.cpp
                       compiler/parser/*.cc
                       compiler/parser/ast/*.cc
                       compiler/parser/ast/transform/*.cc
                       compiler/parser/ast/codegen/*.cc
                       compiler/util/format.cc)

add_library(seq SHARED ${SEQ_HFILES} ${SEQ_CPPFILES} ${LIB_SEQPARSE})
llvm_map_components_to_libnames(LLVM_LIBS support core passes irreader x86asmparser x86info x86codegen mcjit orcjit ipo coroutines)
target_link_libraries(seq ${LLVM_LIBS} dl seqrt)

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

include(GoogleTest)
gtest_discover_tests(seqtest)
enable_testing()
