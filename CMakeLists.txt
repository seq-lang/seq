cmake_minimum_required(VERSION 3.9)
project(Seq)
include(ExternalProject)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fvisibility-inlines-hidden -pedantic -Wall -Wno-return-type-c-linkage")
set(CMAKE_CXX_FLAGS_DEBUG "-g")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")
include_directories(.)
include_directories(compiler)

# set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address")
# set(CMAKE_LINKER_FLAGS_DEBUG "${CMAKE_LINKER_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address")

if(NOT DEFINED SEQ_DEP)
  message(FATAL_ERROR "You must set SEQ_DEP environment variable")
endif()
message(STATUS "Dependency directory: ${SEQ_DEP}")

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

find_package(LLVM REQUIRED CONFIG PATHS ${SEQ_DEP} NO_DEFAULT_PATH)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

set(ZLIB ${SEQ_DEP}/lib/libz.a)
if (NOT EXISTS ${ZLIB})
  message(FATAL_ERROR "Cannot find ${ZLIB}")
endif()
message(STATUS "Found zlib: ${ZLIB}")

set(BDWGC ${SEQ_DEP}/lib/libgc.a)
if (NOT EXISTS ${BDWGC})
  message(FATAL_ERROR "Cannot find ${BDWGC}")
endif()
message(STATUS "Found bdwgc: ${BDWGC}")

execute_process(COMMAND ${SEQ_DEP}/bin/ocamlc -where
                RESULT_VARIABLE result
                OUTPUT_VARIABLE OCAML_STDLIB_PATH
                OUTPUT_STRIP_TRAILING_WHITESPACE)
if (result)
  message(FATAL_ERROR "CMake step for ocaml failed: ${result}")
endif()
message(STATUS "Found OCaml: ${OCAML_STDLIB_PATH}")

set(MENHIRLIB ${SEQ_DEP}/share/menhir)
if (NOT EXISTS ${MENHIRLIB}/menhirLib.cmx)
  message(FATAL_ERROR "Cannot find ${MENHIRLIB}/menhirLib.cmx")
endif()
message(STATUS "Found Menhir: ${MENHIRLIB}")

if (APPLE)
  set(OMPLIB ${SEQ_DEP}/lib/libomp.dylib)
else()
  set(OMPLIB ${SEQ_DEP}/lib/libomp.so)
endif()
if (NOT EXISTS ${OMPLIB})
  message(FATAL_ERROR "Cannot find ${OMPLIB}")
endif()
message(STATUS "Found OpenMP: ${OMPLIB}")
add_library(seqomp SHARED IMPORTED)
set_target_properties(seqomp PROPERTIES IMPORTED_LOCATION ${OMPLIB})

set(CMAKE_BUILD_WITH_INSTALL_RPATH ON)
if(APPLE)
    set(CMAKE_INSTALL_RPATH "@loader_path;@loader_path/../lib/seq")
    set(STATIC_LIBCPP "")
else()
    set(CMAKE_INSTALL_RPATH "$ORIGIN:$ORIGIN/../lib/seq")
    set(STATIC_LIBCPP "-static-libstdc++")
endif()

# Seq runtime library
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
add_library(seqrt SHARED ${SEQRT_FILES})
target_include_directories(seqrt PRIVATE ${SEQ_DEP}/include runtime)
if (APPLE)
  target_link_libraries(seqrt PUBLIC seqomp ${STATIC_LIBCPP} -Wl,-force_load,${ZLIB} -Wl,-force_load,${BDWGC})
else()
  target_link_libraries(seqrt PUBLIC seqomp ${STATIC_LIBCPP} -Wl,--whole-archive ${ZLIB} ${BDWGC} -Wl,--no-whole-archive)
endif()

# Seq parsing library
include_directories(${OCAML_STDLIB_PATH})
link_directories(${OCAML_STDLIB_PATH})

ExternalProject_Add(seqparse_target
  SOURCE_DIR ${CMAKE_SOURCE_DIR}/compiler/parser/ocaml
  CONFIGURE_COMMAND cp -r ${CMAKE_SOURCE_DIR}/compiler/parser/ocaml ${CMAKE_BINARY_DIR}/seqparse
  BINARY_DIR ${CMAKE_BINARY_DIR}/seqparse
  BUILD_COMMAND make -C ocaml
    OCAML=${SEQ_DEP}/bin/ocamlopt
    OCAMLLEX=${SEQ_DEP}/bin/ocamllex
    MENHIR=${SEQ_DEP}/bin/menhir
    MENHIR_LIB=${MENHIRLIB}
  INSTALL_COMMAND "")
ExternalProject_Get_Property(seqparse_target BINARY_DIR)
set(LIB_SEQPARSE ${BINARY_DIR}/ocaml/seqparser.o)
set_property(SOURCE ${LIB_SEQPARSE} PROPERTY GENERATED 1)

# Seq compiler library
include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})
set(SEQ_HPPFILES  compiler/lang/expr.h
                  compiler/lang/func.h
                  compiler/lang/lang.h
                  compiler/lang/patterns.h
                  compiler/lang/pipeline.h
                  compiler/lang/seq.h
                  compiler/lang/stmt.h
                  compiler/lang/var.h
                  compiler/parser/ast/ast/expr.h
                  compiler/parser/ast/ast/stmt.h
                  compiler/parser/ast/ast/pattern.h
                  compiler/parser/ast/ast/visitor.h
                  compiler/parser/ast/codegen/codegen.h
                  compiler/parser/ast/codegen/codegen_ctx.h
                  compiler/parser/ast/context.h
                  compiler/parser/ast/doc/doc.h
                  compiler/parser/ast/format/format.h
                  compiler/parser/ast/transform/transform.h
                  compiler/parser/ast/transform/transform_ctx.h
                  compiler/parser/ast/typecheck/typecheck.h
                  compiler/parser/ast/typecheck/typecheck_ctx.h
                  compiler/parser/ast/types.h
                  compiler/parser/ast/walk.h
                  compiler/parser/common.h
                  compiler/parser/ocaml.h
                  compiler/parser/parser.h
                  compiler/types/any.h
                  compiler/types/array.h
                  compiler/types/base.h
                  compiler/types/func.h
                  compiler/types/num.h
                  compiler/types/optional.h
                  compiler/types/ptr.h
                  compiler/types/record.h
                  compiler/types/ref.h
                  compiler/types/seq.h
                  compiler/types/types.h
                  compiler/types/union.h
                  compiler/types/void.h
                  compiler/util/common.h
                  compiler/util/fmt/chrono.h
                  compiler/util/fmt/color.h
                  compiler/util/fmt/compile.h
                  compiler/util/fmt/core.h
                  compiler/util/fmt/format-inl.h
                  compiler/util/fmt/format.h
                  compiler/util/fmt/locale.h
                  compiler/util/fmt/ostream.h
                  compiler/util/fmt/posix.h
                  compiler/util/fmt/printf.h
                  compiler/util/fmt/ranges.h
                  compiler/util/jit.h
                  compiler/util/llvm.h
                  compiler/util/tapir.h)
set(SEQ_CPPFILES  compiler/lang/expr.cpp
                  compiler/lang/func.cpp
                  compiler/lang/lang.cpp
                  compiler/lang/patterns.cpp
                  compiler/lang/pipeline.cpp
                  compiler/lang/seq.cpp
                  compiler/lang/stmt.cpp
                  compiler/lang/var.cpp
                  compiler/parser/ast/ast/expr.cpp
                  compiler/parser/ast/ast/stmt.cpp
                  compiler/parser/ast/ast/pattern.cpp
                  compiler/parser/ast/ast/visitor.cpp
                  compiler/parser/ast/codegen/codegen.cpp
                  compiler/parser/ast/codegen/codegen_ctx.cpp
                  compiler/parser/ast/context.cpp
                  compiler/parser/ast/doc/doc.cpp
                  compiler/parser/ast/format/format.cpp
                  compiler/parser/ast/transform/transform.cpp
                  compiler/parser/ast/transform/transform_ctx.cpp
                  compiler/parser/ast/typecheck/typecheck.cpp
                  compiler/parser/ast/typecheck/typecheck_ctx.cpp
                  compiler/parser/ast/types.cpp
                  compiler/parser/ast/walk.cpp
                  compiler/parser/common.cpp
                  compiler/parser/ocaml.cpp
                  compiler/parser/parser.cpp
                  compiler/types/any.cpp
                  compiler/types/array.cpp
                  compiler/types/base.cpp
                  compiler/types/func.cpp
                  compiler/types/num.cpp
                  compiler/types/optional.cpp
                  compiler/types/ptr.cpp
                  compiler/types/record.cpp
                  compiler/types/ref.cpp
                  compiler/types/seq.cpp
                  compiler/types/types.cpp
                  compiler/types/union.cpp
                  compiler/types/void.cpp
                  compiler/util/fmt/format.cpp
                  compiler/util/jit.cpp)
add_library(seq SHARED ${SEQ_HPPFILES})
add_dependencies(seq seqparse_target)
target_sources(seq PRIVATE ${LIB_SEQPARSE} ${SEQ_CPPFILES})
llvm_map_components_to_libnames(LLVM_LIBS support core passes irreader x86asmparser x86info x86codegen mcjit orcjit ipo coroutines)
if (APPLE)
  target_link_libraries(seq ${LLVM_LIBS} dl seqrt)
else()
  target_link_libraries(seq ${STATIC_LIBCPP} ${LLVM_LIBS} dl seqrt)
endif()

# Seq command-line tool
add_executable(seqc runtime/main.cpp)
target_link_libraries(seqc ${STATIC_LIBCPP} seq Threads::Threads)

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
target_include_directories(seqtest PRIVATE ${SEQ_DEP}/include)
target_link_libraries(seqtest seq gtest_main)
target_compile_definitions(seqtest PRIVATE TEST_DIR="${CMAKE_CURRENT_SOURCE_DIR}/test")

include(GoogleTest)
gtest_discover_tests(seqtest)
enable_testing()
