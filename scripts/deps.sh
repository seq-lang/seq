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

echo "Dependency generation done: ${INSTALLDIR}"
