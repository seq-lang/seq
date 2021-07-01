#!/usr/bin/env bash

set -e

export CC="${CC:-clang}"
export CXX="${CXX:-clang++}"
export USE_ZLIBNG="${USE_ZLIBNG:-1}"

export INSTALLDIR="${PWD}/deps"
export SRCDIR="${PWD}/deps_src"
mkdir -p "${INSTALLDIR}" "${SRCDIR}"

export LD_LIBRARY_PATH="${INSTALLDIR}/lib:${LD_LIBRARY_PATH}"
export LIBRARY_PATH="${INSTALLDIR}/lib:${LIBRARY_PATH}"
export CPATH="${INSTALLDIR}/include:${CPATH}"

die() { echo "$*" 1>&2 ; exit 1; }

export JOBS=1
if [ -n "${1}" ]; then export JOBS="${1}"; fi
echo "Using ${JOBS} cores..."

# Tapir
TAPIR_LLVM_BRANCH='release_60-release'
if [ ! -d "${SRCDIR}/Tapir-LLVM" ]; then
  git clone --depth 1 -b "${TAPIR_LLVM_BRANCH}" https://github.com/seq-lang/Tapir-LLVM "${SRCDIR}/Tapir-LLVM"
fi
if [ ! -f "${INSTALLDIR}/bin/llc" ]; then
  mkdir -p "${SRCDIR}/Tapir-LLVM/build"
  cd "${SRCDIR}/Tapir-LLVM/build"
  cmake .. \
     -DLLVM_INCLUDE_TESTS=OFF \
     -DLLVM_ENABLE_RTTI=ON \
     -DCMAKE_BUILD_TYPE=Release \
     -DLLVM_TARGETS_TO_BUILD=host \
     -DLLVM_ENABLE_ZLIB=OFF \
     -DLLVM_ENABLE_TERMINFO=OFF \
     -DCMAKE_C_COMPILER="${CC}" \
     -DCMAKE_CXX_COMPILER="${CXX}" \
     -DCMAKE_INSTALL_PREFIX="${INSTALLDIR}"
  make -j "${JOBS}"
  make install
  "${INSTALLDIR}/bin/llvm-config" --cmakedir
fi

# OCaml
OCAML_VERSION='4.07.1'
OCAMLBUILD_VERSION='0.12.0'
if [ ! -d "${SRCDIR}/ocaml-${OCAML_VERSION}" ]; then
  curl -L "https://github.com/ocaml/ocaml/archive/${OCAML_VERSION}.tar.gz" | tar zxf - -C "${SRCDIR}"
fi
if [ ! -f "${INSTALLDIR}/bin/ocamlbuild" ]; then
  cd "${SRCDIR}/ocaml-${OCAML_VERSION}"
  ./configure \
      -cc "gcc -Wno-implicit-function-declaration" \
      -fPIC \
      -no-pthread \
      -no-debugger \
      -no-debug-runtime \
      -prefix "${INSTALLDIR}"
  make -j "${JOBS}" world.opt
  make install
  export PATH="${INSTALLDIR}/bin:${PATH}"

  curl -L "https://github.com/ocaml/ocamlbuild/archive/${OCAMLBUILD_VERSION}.tar.gz" | tar zxf - -C "${SRCDIR}"
  cd "${SRCDIR}/ocamlbuild-${OCAMLBUILD_VERSION}"
  make configure \
    PREFIX="${INSTALLDIR}" \
    OCAMLBUILD_BINDIR="${INSTALLDIR}/bin" \
    OCAMLBUILD_LIBDIR="${INSTALLDIR}/lib" \
    OCAMLBUILD_MANDIR="${INSTALLDIR}/man"
  make -j "${JOBS}"
  make install
  "${INSTALLDIR}/bin/ocaml" -version
  "${INSTALLDIR}/bin/ocamlbuild" -version
fi

export PATH=${INSTALLDIR}/bin:${PATH}

# Menhir
MENHIR_VERSION='20190924'
if [ ! -d "${SRCDIR}/menhir-${MENHIR_VERSION}" ]; then
  curl -L "https://gitlab.inria.fr/fpottier/menhir/-/archive/${MENHIR_VERSION}/menhir-${MENHIR_VERSION}.tar.gz" | tar zxf - -C "${SRCDIR}"
fi
cd "${SRCDIR}/menhir-${MENHIR_VERSION}"
make PREFIX="${INSTALLDIR}" all -j "${JOBS}"
make PREFIX="${INSTALLDIR}" install
"${INSTALLDIR}/bin/menhir" --version
[ ! -f "${INSTALLDIR}/share/menhir/menhirLib.cmx" ] && die "Menhir library not found"

if [ "${USE_ZLIBNG}" = '1' ] ; then
    # zlib-ng
    ZLIBNG_VERSION='2.0.2'
    curl -L "https://github.com/zlib-ng/zlib-ng/archive/${ZLIBNG_VERSION}.tar.gz" | tar zxf - -C "${SRCDIR}"
    cd "${SRCDIR}/zlib-ng-${ZLIBNG_VERSION}"
    CFLAGS="-fPIC" ./configure \
        --64 \
        --zlib-compat \
        --prefix="${INSTALLDIR}"
    make -j "${JOBS}"
    make install
    [ ! -f "${INSTALLDIR}/lib/libz.a" ] && die "zlib (zlib-ng) library not found"
else
    # zlib
    ZLIB_VERSION='1.2.11'
    curl -L "https://zlib.net/zlib-${ZLIB_VERSION}.tar.gz" | tar zxf - -C "${SRCDIR}"
    cd "${SRCDIR}/zlib-${ZLIB_VERSION}"
    CFLAGS="-fPIC" ./configure \
        --64 \
        --static \
        --shared \
        --prefix="${INSTALLDIR}"
    make -j "${JOBS}"
    make install
    [ ! -f "${INSTALLDIR}/lib/libz.a" ] && die "zlib library not found"
fi

# libdeflate
LIBDEFLATE_VERSION='1.7'
curl -L "https://github.com/ebiggers/libdeflate/archive/refs/tags/v${LIBDEFLATE_VERSION}.tar.gz" | tar zxf - -C "${SRCDIR}"
cd "${SRCDIR}/libdeflate-${LIBDEFLATE_VERSION}"
make -j "${JOBS}" -e libdeflate.a CFLAGS="-fPIC" PREFIX="${INSTALLDIR}"
install -m644 libdeflate.a "${INSTALLDIR}/lib"
install -m644 libdeflate.h "${INSTALLDIR}/include"

# bdwgc
BDWGC_VERSION='8.0.4'
curl -L "https://github.com/ivmai/bdwgc/releases/download/v${BDWGC_VERSION}/gc-${BDWGC_VERSION}.tar.gz" | tar zxf - -C "${SRCDIR}"
cd "${SRCDIR}/gc-${BDWGC_VERSION}"
./configure \
    CFLAGS="-fPIC" \
    --enable-threads=posix \
    --enable-large-config \
    --enable-thread-local-alloc \
    --prefix="${INSTALLDIR}"
    # --enable-handle-fork=yes --disable-shared --enable-static
make -j "${JOBS}" LDFLAGS=-static
make install
[ ! -f "${INSTALLDIR}/lib/libgc.a" ] && die "gc library not found"

# htslib
HTSLIB_VERSION='1.12'
curl -L "https://github.com/samtools/htslib/releases/download/${HTSLIB_VERSION}/htslib-${HTSLIB_VERSION}.tar.bz2" | tar jxf - -C "${SRCDIR}"
cd "${SRCDIR}/htslib-${HTSLIB_VERSION}"
# Get needed fix so HTSlib works with zlib-ng: https://github.com/samtools/htslib/compare/develop...jkbonfield:zlib-ng-fix
curl -L -O https://raw.githubusercontent.com/jkbonfield/htslib/715056cdd3f85855a503ac932f58e84b92c7dd0e/bgzf.c
./configure \
    CFLAGS="-fPIC" \
    --disable-libcurl \
    --prefix="${INSTALLDIR}"
make -j "${JOBS}"
make install
[ ! -f "${INSTALLDIR}/lib/libhts.a" ] && die "htslib library not found"

# openmp
OPENMP_BRANCH='release_60'
git clone https://github.com/llvm-mirror/openmp -b "${OPENMP_BRANCH}" "${SRCDIR}/openmp"
mkdir -p "${SRCDIR}/openmp/build"
cd "${SRCDIR}/openmp/build"
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_C_COMPILER="${CC}" \
    -DCMAKE_CXX_COMPILER="${CXX}" \
    -DCMAKE_INSTALL_PREFIX="${INSTALLDIR}" \
    -DOPENMP_ENABLE_LIBOMPTARGET=0
make -j "${JOBS}"
make install
# [ ! -f "${INSTALLDIR}/lib/libomp.so" ] && die "openmp library not found"

# libbacktrace
git clone https://github.com/seq-lang/libbacktrace "${SRCDIR}/libbacktrace"
cd "${SRCDIR}/libbacktrace"
CFLAGS="-fPIC" ./configure --prefix="${INSTALLDIR}"
make -j "${JOBS}"
make install
[ ! -f "${INSTALLDIR}/lib/libbacktrace.a" ] && die "libbacktrace library not found"

echo "Dependency generation done: ${INSTALLDIR}"
