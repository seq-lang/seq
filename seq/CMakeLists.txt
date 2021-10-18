cmake_minimum_required(VERSION 3.14)
project(Seq)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pedantic -Wall -Wno-return-type-c-linkage")
set(CMAKE_CXX_FLAGS_DEBUG "-g -fno-limit-debug-info")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")
include_directories(.)

set(CPM_DOWNLOAD_VERSION 0.32.3)
set(CPM_DOWNLOAD_LOCATION "${CMAKE_BINARY_DIR}/cmake/CPM_${CPM_DOWNLOAD_VERSION}.cmake")
if(NOT (EXISTS ${CPM_DOWNLOAD_LOCATION}))
    message(STATUS "Downloading CPM.cmake...")
    file(DOWNLOAD https://github.com/TheLartians/CPM.cmake/releases/download/v${CPM_DOWNLOAD_VERSION}/CPM.cmake ${CPM_DOWNLOAD_LOCATION})
endif()
include(${CPM_DOWNLOAD_LOCATION})

CPMAddPackage(
    NAME xz
    GITHUB_REPOSITORY "xz-mirror/xz"
    VERSION 5.2.5
    GIT_TAG e7da44d5151e21f153925781ad29334ae0786101
    OPTIONS "BUILD_SHARED_LIBS OFF"
            "CMAKE_POSITION_INDEPENDENT_CODE ON")
if(xz_ADDED)
    set_target_properties(xz PROPERTIES EXCLUDE_FROM_ALL ON)
    set_target_properties(xzdec PROPERTIES EXCLUDE_FROM_ALL ON)
endif()

CPMAddPackage(
    NAME htslib
    VERSION 1.13
    URL "https://github.com/samtools/htslib/releases/download/1.13/htslib-1.13.tar.bz2"
    DOWNLOAD_ONLY YES)
if(htslib_ADDED)
    configure_file(
        ${CMAKE_SOURCE_DIR}/htslib-config.h.cmake
        ${htslib_SOURCE_DIR}/config.h
        COPYONLY)
    write_file(${htslib_SOURCE_DIR}/version.h
        "#define HTS_VERSION_TEXT \"${CPM_PACKAGE_htslib_VERSION}\"")
    write_file(${htslib_SOURCE_DIR}/config_vars.h
        "#define HTS_CC \"\"\n"
        "#define HTS_CPPFLAGS \"\"\n"
        "#define HTS_CFLAGS \"\"\n"
        "#define HTS_LDFLAGS \"\"\n"
        "#define HTS_LIBS \"\"\n")
    add_library(htslib STATIC
        "${htslib_SOURCE_DIR}/kfunc.c"
        "${htslib_SOURCE_DIR}/kstring.c"
        "${htslib_SOURCE_DIR}/bcf_sr_sort.c"
        "${htslib_SOURCE_DIR}/bgzf.c"
        "${htslib_SOURCE_DIR}/errmod.c"
        "${htslib_SOURCE_DIR}/faidx.c"
        "${htslib_SOURCE_DIR}/header.c"
        "${htslib_SOURCE_DIR}/hfile.c"
        "${htslib_SOURCE_DIR}/hts.c"
        "${htslib_SOURCE_DIR}/hts_expr.c"
        "${htslib_SOURCE_DIR}/hts_os.c"
        "${htslib_SOURCE_DIR}/md5.c"
        "${htslib_SOURCE_DIR}/multipart.c"
        "${htslib_SOURCE_DIR}/probaln.c"
        "${htslib_SOURCE_DIR}/realn.c"
        "${htslib_SOURCE_DIR}/regidx.c"
        "${htslib_SOURCE_DIR}/region.c"
        "${htslib_SOURCE_DIR}/sam.c"
        "${htslib_SOURCE_DIR}/synced_bcf_reader.c"
        "${htslib_SOURCE_DIR}/vcf_sweep.c"
        "${htslib_SOURCE_DIR}/tbx.c"
        "${htslib_SOURCE_DIR}/textutils.c"
        "${htslib_SOURCE_DIR}/thread_pool.c"
        "${htslib_SOURCE_DIR}/vcf.c"
        "${htslib_SOURCE_DIR}/vcfutils.c"
        "${htslib_SOURCE_DIR}/cram/cram_codecs.c"
        "${htslib_SOURCE_DIR}/cram/cram_decode.c"
        "${htslib_SOURCE_DIR}/cram/cram_encode.c"
        "${htslib_SOURCE_DIR}/cram/cram_external.c"
        "${htslib_SOURCE_DIR}/cram/cram_index.c"
        "${htslib_SOURCE_DIR}/cram/cram_io.c"
        "${htslib_SOURCE_DIR}/cram/cram_stats.c"
        "${htslib_SOURCE_DIR}/cram/mFILE.c"
        "${htslib_SOURCE_DIR}/cram/open_trace_file.c"
        "${htslib_SOURCE_DIR}/cram/pooled_alloc.c"
        "${htslib_SOURCE_DIR}/cram/string_alloc.c"
        "${htslib_SOURCE_DIR}/htscodecs/htscodecs/arith_dynamic.c"
        "${htslib_SOURCE_DIR}/htscodecs/htscodecs/fqzcomp_qual.c"
        "${htslib_SOURCE_DIR}/htscodecs/htscodecs/htscodecs.c"
        "${htslib_SOURCE_DIR}/htscodecs/htscodecs/pack.c"
        "${htslib_SOURCE_DIR}/htscodecs/htscodecs/rANS_static4x16pr.c"
        "${htslib_SOURCE_DIR}/htscodecs/htscodecs/rANS_static.c"
        "${htslib_SOURCE_DIR}/htscodecs/htscodecs/rle.c"
        "${htslib_SOURCE_DIR}/htscodecs/htscodecs/tokenise_name3.c")
    target_include_directories(htslib BEFORE PRIVATE "${htslib_SOURCE_DIR}" "${xz_SOURCE_DIR}/src/liblzma/api")
    set_target_properties(htslib PROPERTIES
        POSITION_INDEPENDENT_CODE ON
        VISIBILITY_INLINES_HIDDEN ON)
endif()

set(SEQ_FILES
    seq.cpp
    seq.h
    ir/pipeline.cpp
    ir/pipeline.h
    ir/revcomp.cpp
    ir/revcomp.h
    ir/seq.cpp
    ir/seq.h
    sw/ksw2.h
    sw/ksw2_extd2_sse.cpp
    sw/ksw2_exts2_sse.cpp
    sw/ksw2_extz2_sse.cpp
    sw/ksw2_gg2_sse.cpp
    sw/intersw.h
    sw/intersw.cpp)
add_library(seq SHARED ${SEQ_FILES})
add_dependencies(seq htslib)
set_source_files_properties(sw/intersw.cpp PROPERTIES COMPILE_FLAGS -mavx)
if(APPLE)
    target_link_libraries(seq PRIVATE
      -Wl,-force_load,$<TARGET_FILE:htslib>)
else()
    target_link_libraries(seq PRIVATE
      -Wl,--whole-archive $<TARGET_FILE:htslib> -Wl,--no-whole-archive)
endif()

find_package(LLVM REQUIRED CONFIG)
separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND ${LLVM_DEFINITIONS})
add_definitions(${LLVM_DEFINITIONS_LIST})

if(NOT CODON_PATH)
    set(CODON_PATH "$ENV{HOME}/.codon")
endif()

target_include_directories(seq PRIVATE "${CODON_PATH}/include" ${LLVM_INCLUDE_DIRS})
target_link_directories(seq PRIVATE "${CODON_PATH}/lib/codon")
target_link_libraries(seq PRIVATE codonrt codonc)

# Download and unpack googletest at configure time
include(FetchContent)
FetchContent_Declare(googletest
  URL https://github.com/google/googletest/archive/609281088cfefc76f9d0ce82e1ff6c30cc3591e5.zip)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)
enable_testing()
add_executable(seqtest test/main.cpp)
target_include_directories(seqtest PRIVATE "${CODON_PATH}/include" ${LLVM_INCLUDE_DIRS})
target_link_directories(seqtest PRIVATE "${CODON_PATH}/lib/codon")
target_link_libraries(seqtest codonrt codonc gtest_main)
target_compile_definitions(seqtest PRIVATE TEST_DIR="${CMAKE_CURRENT_SOURCE_DIR}/test")
