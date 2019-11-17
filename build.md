# Building from source 

Unless you really need to build Seq for whatever reason, we strongly recommend
using [pre-built binaries](https://github.com/seq-lang/seq/releases). 

Really.

## Dependencies

- Linux or macOS
- [CMake](https://cmake.org) 3.10+
- [LLVM](https://llvm.org) 6.0
- [OCaml](https://ocaml.org) 4.08
- [Boehm GC](https://github.com/ivmai/bdwgc) 7.6+
- [HTSlib](https://htslib.org) 1.9+
- [libffi](https://sourceware.org/libffi) 3.2+
- zlib 
- bz2
- Python 3.6+
- git

#### Platform-independent notes

Ideally, you should build Seq with LLVM-Tapir 6.0.

However, there are no pre-compiled binaries for LLVM-Tapir 6.0, 
and you need to build it yourself. In order to do so, 
please consult the `before_install` section
of [our Travis script](.travis.yml).

Building LLVM takes a lot of time (expect one hour at least).
If you are in rush, just use the system-provided LLVM distribution.
However, please be advised that in that case:

1. parallelism will not work, and
2. performance might be negatively affected for any LLVM other than LLVM 6.0 
   due to the [coroutine regression bug](https://bugs.llvm.org/show_bug.cgi?id=40656) 
   (also discussed [here](https://www.reddit.com/r/cpp/comments/aoad7l/coroutine_allocation_elision_broken_in_clang_7)).

OPAM can also be problematic: if you get into any trouble with OCaml/OPAM, 
get the latest version of OPAM through:

```bash
sh <(curl -sL https://raw.githubusercontent.com/ocaml/opam/master/shell/install.sh)
```

#### macOS:

Install the dependencies via:
```bash
brew install cmake pkg-config llvm@6 opam libffi zlib bzip2 python git xz
```

#### Ubuntu/Debian:

Install the dependencies via:
```bash
apt install cmake pkg-config clang-6.0 llvm-6.0 zlib1g-dev libbz2-dev opam libffi-dev python3 git liblzma-dev
```

#### CentOS/RHEL:

Install the dependencies via:
```bash
yum install cmake pkg-config llvm-toolset llvm-devel llvm-static zlib-devel bzip2-devel libffi-devel python3 git bubblewrap unzip xz-devel
```

To install OPAM, do
```
sh <(curl -sL https://raw.githubusercontent.com/ocaml/opam/master/shell/install.sh)
```

You might need to enable `devtools` repository to be able to install LLVM. 
As instructions depend on your CentOS/RHEL version, please consult the distribution manual.

### OPAM setup

To set up OPAM, do:

```
opam init --bare
opam switch install 4.08.1
eval $(opam env)
opam install core dune menhir ctypes ctypes-foreign ppx_deriving
```

## Building

First prepare the environment:

```bash
mkdir seq/build
cd seq/build
```

Then build the necessary dependencies:

```bash
# Build bdw-gc
curl -L https://www.hboehm.info/gc/gc_source/gc-8.0.4.tar.gz | tar zxvf -
cd gc-8.0.4
mkdir -p rel
./configure --prefix=`pwd`/rel --enable-threads=posix --enable-cplusplus --enable-thread-local-alloc --enable-large-config
make LDFLAGS="-static"
make install
cd ..

# Build HTSlib
curl -L https://github.com/samtools/htslib/releases/download/1.9/htslib-1.9.tar.bz2 | tar jxvf -
cd htslib-1.9
./configure CFLAGS="-fPIC" --disable-libcurl
make
cd ..
```

Then build Seq via:

```bash
cmake .. -DLLVM_DIR=`llvm-config --cmakedir` -DHTS_LIB=htslib-1.9/libhts.a -DGC_LIB=gc-8.0.4/rel/lib/libgc.a
CPATH=gc-8.0.4/rel/include:htslib-1.9 cmake --build .
```

This will produce a `seqc` executable for compiling/running Seq programs, and a `seqtest` executable for running the test suite.
