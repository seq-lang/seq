#!/bin/sh -l

set -e

# setup
cd /github/workspace
apt-get update
apt-get -y --force-yes install build-essential sudo curl wget git zlib1g-dev libbz2-dev liblzma-dev python-software-properties apt-transport-https ca-certificates python3.6

# llvm
wget https://apt.llvm.org/llvm.sh
chmod +x llvm.sh
sudo ./llvm.sh 12

# env
export CC=clang-12
export CXX=clang++-12
export PYTHONPATH=$(pwd)/test/python
export SEQ_PYTHON=$(python3.6 test/python/find-python-library.py)
python3.6 -m pip install numpy

# build
mkdir build
(cd build && cmake .. -DCMAKE_BUILD_TYPE=Release \
                      -DLLVM_DIR=$(llvm-config-12 --cmakedir) \
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
export SEQ_DEP_ARCHIVE=seq-deps-linux.tar.bz2
mkdir -p seq-deploy/bin seq-deploy/lib/seq
cp build/seqc seq-deploy/bin/
cp build/libseq*.so seq-deploy/lib/seq/
cp deps/lib/libomp.so seq-deploy/lib/seq/
cp -r stdlib seq-deploy/lib/seq/
tar -czf ${SEQ_BUILD_ARCHIVE} seq-deploy
tar -cjf ${SEQ_DEP_ARCHIVE} deps
du -sh seq-deploy
du -sh deps
ls -lah ${SEQ_DEP_ARCHIVE}
