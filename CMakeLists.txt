cmake_minimum_required(VERSION 3.9)
project(Seq)
include(ExternalProject)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fvisibility-inlines-hidden -pedantic -mavx -Wall -Wno-return-type-c-linkage")
set(CMAKE_CXX_FLAGS_DEBUG "-g -fno-limit-debug-info")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")
include_directories(.)
include_directories(compiler)

#set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address")
#set(CMAKE_LINKER_FLAGS_DEBUG "${CMAKE_LINKER_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address")

if (NOT DEFINED SEQ_DEP)
    message(FATAL_ERROR "You must set SEQ_DEP environment variable")
endif ()
message(STATUS "Dependency directory: ${SEQ_DEP}")

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

find_package(LLVM REQUIRED CONFIG PATHS ${SEQ_DEP} NO_DEFAULT_PATH)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

set(ZLIB ${SEQ_DEP}/lib/libz.a)
if (NOT EXISTS ${ZLIB})
    message(FATAL_ERROR "Cannot find ${ZLIB}")
endif ()
message(STATUS "Found zlib: ${ZLIB}")

set(BDWGC ${SEQ_DEP}/lib/libgc.a)
if (NOT EXISTS ${BDWGC})
    message(FATAL_ERROR "Cannot find ${BDWGC}")
endif ()
message(STATUS "Found bdwgc: ${BDWGC}")

execute_process(COMMAND ${SEQ_DEP}/bin/ocamlc -where
        RESULT_VARIABLE result
        OUTPUT_VARIABLE OCAML_STDLIB_PATH
        OUTPUT_STRIP_TRAILING_WHITESPACE)
if (result)
    message(FATAL_ERROR "CMake step for ocaml failed: ${result}")
endif ()
message(STATUS "Found OCaml: ${OCAML_STDLIB_PATH}")

set(MENHIRLIB ${SEQ_DEP}/share/menhir)
if (NOT EXISTS ${MENHIRLIB}/menhirLib.cmx)
    message(FATAL_ERROR "Cannot find ${MENHIRLIB}/menhirLib.cmx")
endif ()
message(STATUS "Found Menhir: ${MENHIRLIB}")

if (APPLE)
    set(OMPLIB ${SEQ_DEP}/lib/libomp.dylib)
else ()
    set(OMPLIB ${SEQ_DEP}/lib/libomp.so)
endif ()
if (NOT EXISTS ${OMPLIB})
    message(FATAL_ERROR "Cannot find ${OMPLIB}")
endif ()
message(STATUS "Found OpenMP: ${OMPLIB}")
add_library(seqomp SHARED IMPORTED)
set_target_properties(seqomp PROPERTIES IMPORTED_LOCATION ${OMPLIB})

set(CMAKE_BUILD_WITH_INSTALL_RPATH ON)
if (APPLE)
    set(CMAKE_INSTALL_RPATH "@loader_path;@loader_path/../lib/seq")
    set(STATIC_LIBCPP "")
else ()
    set(CMAKE_INSTALL_RPATH "$ORIGIN:$ORIGIN/../lib/seq")
    set(STATIC_LIBCPP "-static-libstdc++")
endif ()

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
target_compile_options(seqrt PRIVATE -mavx)
target_include_directories(seqrt PRIVATE ${SEQ_DEP}/include runtime)
if (APPLE)
    target_link_libraries(seqrt PUBLIC seqomp ${STATIC_LIBCPP} -Wl,-force_load,${ZLIB} -Wl,-force_load,${BDWGC})
else ()
    target_link_libraries(seqrt PUBLIC seqomp ${STATIC_LIBCPP} -Wl,--whole-archive ${ZLIB} ${BDWGC} -Wl,--no-whole-archive)
endif ()
set(BACKTRACE ${SEQ_DEP}/lib/libbacktrace.a)
target_link_libraries(seqrt PUBLIC ${BACKTRACE})

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

set(SEQ_HPPFILES
        compiler/dsl/dsl.h
        compiler/dsl/plugins.h
        compiler/parser/ast.h
        compiler/parser/ast/expr.h
        compiler/parser/ast/stmt.h
        compiler/parser/ast/types.h
        compiler/parser/cache.h
        compiler/parser/common.h
        compiler/parser/ctx.h
        compiler/parser/ocaml/ocaml.h
        compiler/parser/parser.h
        compiler/parser/visitors/doc/doc.h
        compiler/parser/visitors/format/format.h
        compiler/parser/visitors/simplify/simplify.h
        compiler/parser/visitors/simplify/simplify_ctx.h
        compiler/parser/visitors/translate/translate.h
        compiler/parser/visitors/translate/translate_ctx.h
        compiler/parser/visitors/typecheck/typecheck.h
        compiler/parser/visitors/typecheck/typecheck_ctx.h
        compiler/parser/visitors/visitor.h
        compiler/seq/pipeline.h
        compiler/seq/revcomp.h
        compiler/seq/seq.h
        compiler/sequre/arithmetics.h
        compiler/sequre/sequre.h
        compiler/sir/analyze/analysis.h
        compiler/sir/analyze/dataflow/cfg.h
        compiler/sir/analyze/dataflow/dominator.h
        compiler/sir/analyze/dataflow/reaching.h
        compiler/sir/attribute.h
        compiler/sir/base.h
        compiler/sir/const.h
        compiler/sir/dsl/codegen.h
        compiler/sir/dsl/nodes.h
        compiler/sir/flow.h
        compiler/sir/func.h
        compiler/sir/instr.h
        compiler/sir/llvm/llvisitor.h
        compiler/sir/llvm/llvm.h
        compiler/sir/module.h
        compiler/sir/sir.h
        compiler/sir/transform/cleanup/replacer.h
        compiler/sir/transform/lowering/imperative.h
        compiler/sir/transform/manager.h
        compiler/sir/transform/pass.h
        compiler/sir/transform/pythonic/dict.h
        compiler/sir/transform/pythonic/io.h
        compiler/sir/transform/pythonic/str.h
        compiler/sir/types/types.h
        compiler/sir/util/cloning.h
        compiler/sir/util/context.h
        compiler/sir/util/format.h
        compiler/sir/util/irtools.h
        compiler/sir/util/iterators.h
        compiler/sir/util/matching.h
        compiler/sir/util/operator.h
        compiler/sir/util/packs.h
        compiler/sir/util/visitor.h
        compiler/sir/value.h
        compiler/sir/var.h
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
        compiler/util/fmt/ranges.h)
set(SEQ_CPPFILES
        compiler/dsl/plugins.cpp
        compiler/parser/ast/expr.cpp
        compiler/parser/ast/stmt.cpp
        compiler/parser/ast/types.cpp
        compiler/parser/cache.cpp
        compiler/parser/common.cpp
        compiler/parser/ocaml/ocaml.cpp
        compiler/parser/parser.cpp
        compiler/parser/visitors/doc/doc.cpp
        compiler/parser/visitors/format/format.cpp
        compiler/parser/visitors/simplify/simplify.cpp
        compiler/parser/visitors/simplify/simplify_ctx.cpp
        compiler/parser/visitors/simplify/simplify_expr.cpp
        compiler/parser/visitors/simplify/simplify_stmt.cpp
        compiler/parser/visitors/translate/translate.cpp
        compiler/parser/visitors/translate/translate_ctx.cpp
        compiler/parser/visitors/typecheck/typecheck.cpp
        compiler/parser/visitors/typecheck/typecheck_ctx.cpp
        compiler/parser/visitors/typecheck/typecheck_expr.cpp
        compiler/parser/visitors/typecheck/typecheck_infer.cpp
        compiler/parser/visitors/typecheck/typecheck_stmt.cpp
        compiler/parser/visitors/visitor.cpp
        compiler/seq/pipeline.cpp
        compiler/seq/revcomp.cpp
        compiler/seq/seq.cpp
        compiler/sequre/arithmetics.cpp
        compiler/sequre/sequre.cpp
        compiler/sir/attribute.cpp
        compiler/sir/analyze/analysis.cpp
        compiler/sir/analyze/dataflow/cfg.cpp
        compiler/sir/analyze/dataflow/dominator.cpp
        compiler/sir/analyze/dataflow/reaching.cpp
        compiler/sir/base.cpp
        compiler/sir/const.cpp
        compiler/sir/dsl/nodes.cpp
        compiler/sir/flow.cpp
        compiler/sir/func.cpp
        compiler/sir/instr.cpp
        compiler/sir/llvm/llvisitor.cpp
        compiler/sir/module.cpp
        compiler/sir/transform/cleanup/replacer.cpp
        compiler/sir/transform/lowering/imperative.cpp
        compiler/sir/transform/manager.cpp
        compiler/sir/transform/pass.cpp
        compiler/sir/transform/pythonic/dict.cpp
        compiler/sir/transform/pythonic/io.cpp
        compiler/sir/transform/pythonic/str.cpp
        compiler/sir/types/types.cpp
        compiler/sir/util/cloning.cpp
        compiler/sir/util/format.cpp
        compiler/sir/util/irtools.cpp
        compiler/sir/util/matching.cpp
        compiler/sir/util/visitor.cpp
        compiler/sir/value.cpp
        compiler/sir/var.cpp
        compiler/util/common.cpp
        compiler/util/fmt/format.cpp)
add_library(seq SHARED ${SEQ_HPPFILES})
add_dependencies(seq seqparse_target)
target_sources(seq PRIVATE ${LIB_SEQPARSE} ${SEQ_CPPFILES})
llvm_map_components_to_libnames(LLVM_LIBS support core passes irreader x86asmparser x86info x86codegen mcjit orcjit ipo coroutines)
if (APPLE)
    target_link_libraries(seq ${LLVM_LIBS} dl seqrt)
else ()
    target_link_libraries(seq ${STATIC_LIBCPP} ${LLVM_LIBS} dl seqrt)
endif ()

# Seq command-line tool
add_executable(seqc runtime/main.cpp)
target_link_libraries(seqc ${STATIC_LIBCPP} seq Threads::Threads)

# Seq test
# Download and unpack googletest at configure time
configure_file(test/CMakeLists.txt.in googletest-download/CMakeLists.txt)
execute_process(COMMAND ${CMAKE_COMMAND} -G "${CMAKE_GENERATOR}" .
        RESULT_VARIABLE result
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/googletest-download)
if (result)
    message(FATAL_ERROR "CMake step for googletest failed: ${result}")
endif ()
execute_process(COMMAND ${CMAKE_COMMAND} --build .
        RESULT_VARIABLE result
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/googletest-download)
if (result)
    message(FATAL_ERROR "Build step for googletest failed: ${result}")
endif ()

# Prevent overriding the parent project's compiler/linker
# settings on Windows
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

# Add googletest directly to our build. This defines
# the gtest and gtest_main targets.
add_subdirectory(${CMAKE_CURRENT_BINARY_DIR}/googletest-src
        ${CMAKE_CURRENT_BINARY_DIR}/googletest-build
        EXCLUDE_FROM_ALL)

set(SEQ_TEST_CPPFILES
        test/main.cpp
        test/sir/analyze/dominator.cpp
        test/sir/analyze/reaching.cpp
        test/sir/base.cpp
        test/sir/constant.cpp
        test/sir/flow.cpp
        test/sir/func.cpp
        test/sir/instr.cpp
        test/sir/module.cpp
        test/sir/transform/manager.cpp
        test/sir/types/types.cpp
        test/sir/util/matching.cpp
        test/sir/value.cpp
        test/sir/var.cpp
        test/types.cpp)
add_executable(seqtest ${SEQ_TEST_CPPFILES})
target_include_directories(seqtest PRIVATE ${SEQ_DEP}/include test/sir)
target_link_libraries(seqtest seq gtest_main)
target_compile_definitions(seqtest PRIVATE TEST_DIR="${CMAKE_CURRENT_SOURCE_DIR}/test")

include(GoogleTest)
gtest_discover_tests(seqtest)
enable_testing()
