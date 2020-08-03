cmake_minimum_required(VERSION 3.5)
project(Seq)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fvisibility-inlines-hidden -pedantic -Wall -Wno-return-type-c-linkage")
set(CMAKE_CXX_FLAGS_DEBUG "-g")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")
include_directories(.)
include_directories(compiler)

find_package(LLVM REQUIRED CONFIG)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

# Seq runtime library
option(SEQ_THREADED "compile runtime library for multithreading" OFF)
option(SEQ_JITBRIDGE "support JIT interoperability" OFF)
find_package(ZLIB REQUIRED)

include(ExternalProject)
ExternalProject_Add(bdwgc
  URL https://github.com/ivmai/bdwgc/releases/download/v8.0.4/gc-8.0.4.tar.gz
  SOURCE_DIR ${CMAKE_BINARY_DIR}/bdwgc
  CONFIGURE_COMMAND ${CMAKE_BINARY_DIR}/bdwgc/configure CFLAGS=-fPIC --enable-threads=posix --enable-cplusplus --enable-thread-local-alloc --enable-large-config --prefix=${CMAKE_BINARY_DIR}/bdwgc/build
  BUILD_COMMAND make LDFLAGS=-static
  BUILD_IN_SOURCE ON
  INSTALL_DIR ${CMAKE_BINARY_DIR}/bdwgc/build
  INSTALL_COMMAND make install
  LOG_DOWNLOAD ON
  # LOG_CONFIGURE ON
  # LOG_BUILD ON
  # LOG_INSTALL ON
)
ExternalProject_Get_Property(bdwgc install_dir)
set(BDWGC_DIR ${install_dir})

set(SEQRT_FILES runtime/lib.h
                runtime/lib.cpp
                runtime/exc.cpp
                runtime/sw/ksw2.h
                runtime/sw/ksw2_extd2_sse.cpp
                runtime/sw/ksw2_exts2_sse.cpp
                runtime/sw/ksw2_extz2_sse.cpp
                runtime/sw/ksw2_gg2_sse.cpp
                runtime/sw/intersw.h
                runtime/sw/intersw.cpp)
add_library(seqrt OBJECT ${SEQRT_FILES})
add_dependencies(seqrt bdwgc)
target_include_directories(seqrt PRIVATE runtime ${BDWGC_DIR}/include)
set_source_files_properties(runtime/sw/intersw.cpp PROPERTIES COMPILE_FLAGS "-march=native")
if(SEQ_THREADED)
  target_compile_definitions(seqrt PRIVATE THREADED=1)
else()
  target_compile_definitions(seqrt PRIVATE THREADED=0)
endif()


add_library(seqrt_shared SHARED $<TARGET_OBJECTS:seqrt>)
add_dependencies(seqrt_shared bdwgc)
target_link_libraries(seqrt_shared PUBLIC ${ZLIB_LIBRARIES} ${BDWGC_DIR}/lib/libgc.a Threads::Threads)
if (SEQ_THREADED)
  find_package(OpenMP REQUIRED)
  target_link_libraries(seqrt_shared PUBLIC OpenMP::OpenMP_CXX)
endif()
add_library(seqrt_static STATIC $<TARGET_OBJECTS:seqrt>)
add_dependencies(seqrt_static bdwgc)
target_link_libraries(seqrt_static ${BDWGC_DIR}/lib/libgc.a)

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
ExternalProject_Add(menhir
  URL https://gitlab.inria.fr/fpottier/menhir/-/archive/20190924/menhir-20190924.tar.gz
  SOURCE_DIR ${CMAKE_BINARY_DIR}/menhir
  CONFIGURE_COMMAND ""
  BUILD_COMMAND make -C src -j
  BUILD_IN_SOURCE ON
  INSTALL_COMMAND ""
  LOG_DOWNLOAD ON
  # LOG_BUILD ON
)
ExternalProject_Get_Property(menhir SOURCE_DIR)
set(MENHIR ${SOURCE_DIR}/src/_stage1/menhir.native)
set(MENHIR_LIB ${SOURCE_DIR}/src/_stage1)

ExternalProject_Add(seqparse_target
  SOURCE_DIR ${CMAKE_SOURCE_DIR}/compiler/parser/ocaml
  CONFIGURE_COMMAND cp -r ${CMAKE_SOURCE_DIR}/compiler/parser/ocaml ${CMAKE_BINARY_DIR}/seqparse
  BINARY_DIR ${CMAKE_BINARY_DIR}/seqparse
  BUILD_COMMAND make -C ocaml MENHIR=${MENHIR} MENHIR_LIB=${MENHIR_LIB}
  INSTALL_COMMAND ""
  # LOG_DOWNLOAD ON
  # LOG_BUILD ON
)
ExternalProject_Get_Property(seqparse_target BINARY_DIR)
set(LIB_SEQPARSE ${BINARY_DIR}/ocaml/seqparser.o)
add_dependencies(seqparse_target menhir)
set_property(SOURCE ${LIB_SEQPARSE} PROPERTY GENERATED 1)

# Seq compiler library
include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

file(GLOB SEQ_HPPFILES compiler/lang/*.h
                       compiler/types/*.h
                       compiler/parser/*.h
                       compiler/parser/ast/*.h
                       compiler/parser/ast/transform/*.h
                       compiler/parser/ast/codegen/*.h
                       compiler/parser/ast/format/*.h
                       compiler/util/*.h
                       compiler/util/fmt/*.h)
file(GLOB SEQ_CPPFILES compiler/lang/*.cpp
                       compiler/types/*.cpp
                       compiler/parser/*.cpp
                       compiler/parser/ast/*.cpp
                       compiler/parser/ast/transform/*.cpp
                       compiler/parser/ast/codegen/*.cpp
                       compiler/parser/ast/format/*.cpp
                       compiler/util/*.cpp
                       compiler/util/fmt/*.cpp)

add_library(seq SHARED ${SEQ_HPPFILES})
add_dependencies(seq seqparse_target)
target_sources(seq PRIVATE ${LIB_SEQPARSE} ${SEQ_CPPFILES})
llvm_map_components_to_libnames(LLVM_LIBS support core passes irreader x86asmparser x86info x86codegen mcjit orcjit ipo coroutines)
target_link_libraries(seq ${LLVM_LIBS} seqrt_shared dl)

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
