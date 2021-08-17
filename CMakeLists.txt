cmake_minimum_required(VERSION 3.9)
project(Seq)
include(ExternalProject)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fvisibility-inlines-hidden -pedantic -Wall -Wno-return-type-c-linkage -Wno-gnu-zero-variadic-macro-arguments")
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

include(ExternalProject)
ExternalProject_Add(zlibng
    URL https://github.com/zlib-ng/zlib-ng/archive/2.0.2.tar.gz
    BUILD_IN_SOURCE 1
    UPDATE_DISCONNECTED TRUE
    WORKING_DIRECTORY <SOURCE_DIR>
    CONFIGURE_COMMAND "CFLAGS=-fPIC" ./configure --64 --static --zlib-compat --prefix=<INSTALL_DIR>
    BUILD_COMMAND make -j${CONCURRENCY})
ExternalProject_Get_Property(zlibng INSTALL_DIR)
set(ZLIB ${INSTALL_DIR}/lib/libz.a)

ExternalProject_Add(libbacktrace
    GIT_REPOSITORY https://github.com/seq-lang/libbacktrace
    BUILD_IN_SOURCE 1
    UPDATE_DISCONNECTED TRUE
    WORKING_DIRECTORY <SOURCE_DIR>
    CONFIGURE_COMMAND "CFLAGS=-fPIC" ./configure --prefix=<INSTALL_DIR>
    BUILD_COMMAND make -j${CONCURRENCY})
ExternalProject_Get_Property(libbacktrace INSTALL_DIR)
set(BACKTRACE ${INSTALL_DIR}/lib/libbacktrace.a)
set(BACKTRACE_INCLUDE ${INSTALL_DIR}/include)

ExternalProject_Add(bz2
    URL https://www.sourceware.org/pub/bzip2/bzip2-1.0.8.tar.gz
    BUILD_IN_SOURCE 1
    UPDATE_DISCONNECTED TRUE
    WORKING_DIRECTORY <SOURCE_DIR>
    CONFIGURE_COMMAND ""
    BUILD_COMMAND make "CFLAGS=-Wall -Winline -O2 -g -D_FILE_OFFSET_BITS=64 -fPIC" -j${CONCURRENCY}
    INSTALL_COMMAND make install PREFIX=<INSTALL_DIR>)
ExternalProject_Get_Property(bz2 INSTALL_DIR)
set(BZ2 ${INSTALL_DIR}/lib/libbz2.a)

ExternalProject_Add(xz
    URL https://tukaani.org/xz/xz-5.2.5.tar.gz
    BUILD_IN_SOURCE 1
    UPDATE_DISCONNECTED TRUE
    WORKING_DIRECTORY <SOURCE_DIR>
    CONFIGURE_COMMAND ./configure "CFLAGS=-fPIC"
        --disable-xz
        --disable-xzdec
        --disable-lzmadec
        --disable-lzmainfo
        --disable-shared
        --enable-pic
        --prefix=<INSTALL_DIR>
    BUILD_COMMAND make -j${CONCURRENCY})
ExternalProject_Get_Property(xz INSTALL_DIR)
set(LZMA ${INSTALL_DIR}/lib/liblzma.a)

ExternalProject_Add(htslib
    DEPENDS bz2 xz
    URL https://github.com/samtools/htslib/releases/download/1.13/htslib-1.13.tar.bz2
    BUILD_IN_SOURCE 1
    UPDATE_DISCONNECTED TRUE
    WORKING_DIRECTORY <SOURCE_DIR>
    CONFIGURE_COMMAND ./configure
        "CFLAGS=-fPIC"
        --disable-libcurl
        --without-libdeflate
        --prefix=<INSTALL_DIR>
    BUILD_COMMAND make -j${CONCURRENCY})
ExternalProject_Get_Property(htslib INSTALL_DIR)
set(HTSLIB ${INSTALL_DIR}/lib/libhts.a)

ExternalProject_Add(bdwgc
    GIT_REPOSITORY https://github.com/ivmai/bdwgc
    GIT_TAG d0ba209660ea8c663e06d9a68332ba5f42da54ba
    UPDATE_DISCONNECTED TRUE
    CMAKE_ARGS
        -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
        -DCMAKE_BUILD_TYPE=Release
        -DCMAKE_POSITION_INDEPENDENT_CODE=ON
        -DBUILD_SHARED_LIBS=OFF
        -Denable_threads=ON
        -Denable_large_config=ON
        -Denable_thread_local_alloc=ON
        -Denable_handle_fork=ON)
ExternalProject_Get_Property(bdwgc INSTALL_DIR)
add_custom_command(TARGET bdwgc POST_BUILD
    VERBATIM COMMAND mkdir -p ${INSTALL_DIR}/lib && \[\[ -f ${INSTALL_DIR}/lib64/libgc.a \]\] && cp ${INSTALL_DIR}/lib64/libgc.a ${INSTALL_DIR}/lib || true)
set(BDWGC ${INSTALL_DIR}/lib/libgc.a)

ExternalProject_Add(openmp
        GIT_REPOSITORY https://github.com/llvm-mirror/openmp
        GIT_TAG release_60
        UPDATE_DISCONNECTED TRUE
        CMAKE_ARGS
        -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
        -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
        -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
        -DCMAKE_BUILD_TYPE=Release
        -DOPENMP_ENABLE_LIBOMPTARGET=0)
add_library(seqomp SHARED IMPORTED)
add_dependencies(seqomp openmp)
ExternalProject_Get_Property(openmp INSTALL_DIR)
if (APPLE)
    set(OMPLIB ${INSTALL_DIR}/lib/libomp.dylib)
else ()
    set(OMPLIB ${INSTALL_DIR}/lib/libomp.so)
endif ()
set_target_properties(seqomp PROPERTIES IMPORTED_LOCATION ${OMPLIB})
add_custom_command(TARGET openmp POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy ${OMPLIB} ${CMAKE_BINARY_DIR})

set(CMAKE_BUILD_WITH_INSTALL_RPATH ON)
if (APPLE)
    set(CMAKE_INSTALL_RPATH "@loader_path;@loader_path/../lib/seq")
    set(STATIC_LIBCPP "")
else ()
    set(CMAKE_INSTALL_RPATH "$ORIGIN:$ORIGIN/../lib/seq")
    set(STATIC_LIBCPP "-static-libstdc++")
endif ()

add_executable(peg2cpp compiler/util/peg2cpp.cpp compiler/util/peglib.h)
target_link_libraries(peg2cpp Threads::Threads)
add_custom_command(
    OUTPUT  seq_rules.cpp
    COMMAND peg2cpp ${CMAKE_SOURCE_DIR}/compiler/parser/peg/grammar.peg seq_rules.cpp seq
    DEPENDS peg2cpp compiler/parser/peg/grammar.peg
)
add_custom_command(
    OUTPUT  omp_rules.cpp
    COMMAND peg2cpp ${CMAKE_SOURCE_DIR}/compiler/parser/peg/openmp.peg omp_rules.cpp omp
    DEPENDS peg2cpp compiler/parser/peg/openmp.peg
)

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
add_dependencies(seqrt zlibng bdwgc libbacktrace bz2 htslib xz)
target_compile_options(seqrt PRIVATE -mavx)
target_include_directories(seqrt PRIVATE ${SEQ_DEP}/include ${BACKTRACE_INCLUDE} runtime)
target_link_libraries(seqrt PUBLIC seqomp ${BACKTRACE} ${STATIC_LIBCPP})
if (APPLE)
    target_link_libraries(seqrt PUBLIC -Wl,-force_load,${BZ2} -Wl,-force_load,${LZMA} -Wl,-force_load,${ZLIB} -Wl,-force_load,${BDWGC} -Wl,-force_load,${HTSLIB})
else ()
    target_link_libraries(seqrt PUBLIC -Wl,--whole-archive ${BZ2} ${LZMA} ${ZLIB} ${HTSLIB} ${BDWGC} -Wl,--no-whole-archive)
endif ()

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
        compiler/parser/peg/peg.h
        compiler/parser/peg/rules.h
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
        compiler/sir/analyze/analysis.h
        compiler/sir/analyze/dataflow/cfg.h
        compiler/sir/analyze/dataflow/dominator.h
        compiler/sir/analyze/dataflow/reaching.h
        compiler/sir/analyze/module/global_vars.h
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
        compiler/sir/transform/cleanup/canonical.h
        compiler/sir/transform/cleanup/dead_code.h
        compiler/sir/transform/cleanup/replacer.h
        compiler/sir/transform/folding/const_fold.h
        compiler/sir/transform/folding/const_prop.h
        compiler/sir/transform/folding/folding.h
        compiler/sir/transform/folding/rule.h
        compiler/sir/transform/lowering/imperative.h
        compiler/sir/transform/lowering/pipeline.h
        compiler/sir/transform/manager.h
        compiler/sir/transform/parallel/openmp.h
        compiler/sir/transform/parallel/schedule.h
        compiler/sir/transform/pass.h
        compiler/sir/transform/pythonic/dict.h
        compiler/sir/transform/pythonic/io.h
        compiler/sir/transform/pythonic/str.h
        compiler/sir/transform/rewrite.h
        compiler/sir/types/types.h
        compiler/sir/util/cloning.h
        compiler/sir/util/context.h
        compiler/sir/util/format.h
        compiler/sir/util/inlining.h
        compiler/sir/util/irtools.h
        compiler/sir/util/iterators.h
        compiler/sir/util/matching.h
        compiler/sir/util/operator.h
        compiler/sir/util/outlining.h
        compiler/sir/util/packs.h
        compiler/sir/util/visitor.h
        compiler/sir/value.h
        compiler/sir/var.h
        compiler/util/common.h
        compiler/util/peglib.h
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
        compiler/parser/peg/peg.cpp
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
        compiler/sir/attribute.cpp
        compiler/sir/analyze/analysis.cpp
        compiler/sir/analyze/dataflow/cfg.cpp
        compiler/sir/analyze/dataflow/dominator.cpp
        compiler/sir/analyze/dataflow/reaching.cpp
        compiler/sir/analyze/module/global_vars.cpp
        compiler/sir/base.cpp
        compiler/sir/const.cpp
        compiler/sir/dsl/nodes.cpp
        compiler/sir/flow.cpp
        compiler/sir/func.cpp
        compiler/sir/instr.cpp
        compiler/sir/llvm/llvisitor.cpp
        compiler/sir/module.cpp
        compiler/sir/transform/cleanup/canonical.cpp
        compiler/sir/transform/cleanup/dead_code.cpp
        compiler/sir/transform/cleanup/replacer.cpp
        compiler/sir/transform/folding/const_fold.cpp
        compiler/sir/transform/folding/const_prop.cpp
        compiler/sir/transform/folding/folding.cpp
        compiler/sir/transform/lowering/imperative.cpp
        compiler/sir/transform/lowering/pipeline.cpp
        compiler/sir/transform/manager.cpp
        compiler/sir/transform/parallel/openmp.cpp
        compiler/sir/transform/parallel/schedule.cpp
        compiler/sir/transform/pass.cpp
        compiler/sir/transform/pythonic/dict.cpp
        compiler/sir/transform/pythonic/io.cpp
        compiler/sir/transform/pythonic/str.cpp
        compiler/sir/types/types.cpp
        compiler/sir/util/cloning.cpp
        compiler/sir/util/format.cpp
        compiler/sir/util/inlining.cpp
        compiler/sir/util/irtools.cpp
        compiler/sir/util/matching.cpp
        compiler/sir/util/outlining.cpp
        compiler/sir/util/visitor.cpp
        compiler/sir/value.cpp
        compiler/sir/var.cpp
        compiler/util/common.cpp
        compiler/util/fmt/format.cpp)
add_library(seq SHARED ${SEQ_HPPFILES})
target_sources(seq PRIVATE ${SEQ_CPPFILES} seq_rules.cpp omp_rules.cpp)
if (CMAKE_BUILD_TYPE MATCHES Debug)
    set_source_files_properties(seq_rules.cpp compiler/parser/peg/peg.cpp PROPERTIES COMPILE_FLAGS "-O2")
endif ()
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
include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/609281088cfefc76f9d0ce82e1ff6c30cc3591e5.zip
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)
enable_testing()
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
target_link_libraries(seqtest seq seqrt gtest_main)
target_compile_definitions(seqtest PRIVATE TEST_DIR="${CMAKE_CURRENT_SOURCE_DIR}/test")

include(GoogleTest)
gtest_discover_tests(seqtest)
enable_testing()
