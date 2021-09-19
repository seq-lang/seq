#!/bin/sh -l
set -e

# setup
cd /github/workspace
yum -y update
yum -y install wget python3 llvm-toolset-7

# env
export CC=/opt/rh/llvm-toolset-7/root/usr/bin/clang
export CXX=/opt/rh/llvm-toolset-7/root/usr/bin/clang++
export PYTHONPATH=$(pwd)/test/python
export SEQ_PYTHON=$(python3 test/python/find-python-library.py)
python3 -m pip install numpy

# deps
if [ ! -d ./llvm ]; then
  /bin/bash scripts/deps.sh 2;
fi

# build
mkdir build
export LLVM_DIR=$(llvm/bin/llvm-config --cmakedir)
(cd build && cmake .. -DCMAKE_BUILD_TYPE=Release \
                      -DCMAKE_C_COMPILER=${CC} \
                      -DCMAKE_CXX_COMPILER=${CXX})
cmake --build build --config Release -- VERBOSE=1

# test
ln -s build/libseqrt.so .
build/seqtest
build/seqc run test/core/helloworld.seq
build/seqc run test/core/exit.seq || if [[ $? -ne 42 ]]; then false; fi

# package
export SEQ_BUILD_ARCHIVE=seq-$(uname -s | awk '{print tolower($0)}')-$(uname -m).tar.gz
mkdir -p seq-deploy/bin seq-deploy/lib/seq
cp build/seqc seq-deploy/bin/
cp build/libseq*.so seq-deploy/lib/seq/
cp build/libomp.so seq-deploy/lib/seq/
cp -r stdlib seq-deploy/lib/seq/
tar -czf ${SEQ_BUILD_ARCHIVE} seq-deploy
du -sh seq-deploy
