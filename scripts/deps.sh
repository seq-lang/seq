#!/usr/bin/env bash
set -e

export CC="${CC:-clang}"
export CXX="${CXX:-clang++}"

export INSTALLDIR="llvm"
export SRCDIR="llvm-project"
mkdir -p "${INSTALLDIR}" "${SRCDIR}"

export JOBS=1
if [ -n "${1}" ]; then export JOBS="${1}"; fi
echo "Using ${JOBS} cores..."

LLVM_BRANCH="release/12.x"
if [ ! -f "${INSTALLDIR}/bin/llvm-config" ]; then
  git clone --depth 1 -b "${LLVM_BRANCH}" https://github.com/llvm/llvm-project "${SRCDIR}"
  mkdir -p "${SRCDIR}/llvm/build"
  cd "${SRCDIR}/llvm/build"
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
