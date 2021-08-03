#!/bin/sh -l

set -e

# setup
cd /github/workspace
sed -i.bak -r 's/(archive|security).ubuntu.com/old-releases.ubuntu.com/g' /etc/apt/sources.list
apt-get update
apt-get -y --force-yes install build-essential sudo curl wget git zlib1g-dev libbz2-dev liblzma-dev python-software-properties apt-transport-https ca-certificates
wget -O - https://apt.llvm.org/llvm-snapshot.gpg.key | sudo apt-key add -
sudo add-apt-repository -y ppa:ubuntu-toolchain-r/test
sudo add-apt-repository -y ppa:fkrull/deadsnakes
sudo apt-add-repository 'deb https://apt.llvm.org/precise/ llvm-toolchain-precise-4.0 main'
# workaround: https://github.com/skyportal/skyportal/commit/6e639e4b4af93323095b22bb3994ccc358a4b379
sudo rm -f /etc/apt/sources.list.d/mongodb*
sudo rm -f /etc/apt/sources.list.d/couchdb*
sudo apt-key adv --keyserver keyserver.ubuntu.com --recv-keys 762E3157
sudo apt-get -q update
sudo apt-get -y --force-yes install clang-4.0 clang++-4.0 python3.5 python3.5-dev autoconf libtool
sudo ln -s /usr/bin/clang-4.0 /usr/bin/clang
sudo ln -s /usr/bin/clang++-4.0 /usr/bin/clang++
wget https://github.com/Kitware/CMake/releases/download/v3.18.1/cmake-3.18.1-Linux-x86_64.sh
sudo sh cmake-3.18.1-Linux-x86_64.sh --prefix=/usr --skip-license
wget -q -O - https://bootstrap.pypa.io/pip/3.5/get-pip.py | sudo python3.5
python3.5 -m pip install numpy

export CC=clang
export CXX=clang++

# deps
ls
if [ ! -d ./deps ]; then
  /bin/bash scripts/deps.sh 2;
fi

# env
export PYTHONPATH=$(pwd)/test/python
export SEQ_PATH=$(pwd)/stdlib
export SEQ_PYTHON=$(python3.5 test/python/find-python-library.py)

# build
mkdir build
ln -s $(pwd)/deps/lib/libomp.so $(pwd)/build/libomp.so
(cd build && cmake .. -DCMAKE_BUILD_TYPE=Release \
                      -DSEQ_DEP=$(pwd)/../deps \
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
