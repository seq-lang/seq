# Seq — a language for computational genomics

[![Build Status](https://travis-ci.com/seq-lang/seq.svg?token=QGRVvAxcSasMm4MgJvYL&branch=master)](https://travis-ci.com/seq-lang/seq)


## Dependencies

- C++11
- CMake 3.9+
- LLVM 5+
- Boehm GC
- OCaml
- OpenSSL


## Build

```bash
mkdir seq/build
cd seq/builld
# for macOS brew w/ any version of OpenSSL and LLVM:
LLVM_DIR=/usr/local/opt/llvm/lib/cmake OPENSSL_ROOT_DIR=/usr/local/opt/openssl cmake ..
cmake --build .
```

This will produce a `seq` executable for compiling/running Seq progrms, and a `seqtest` executable for running the test suite.


## Documentation

[Sphinx](http://www.sphinx-doc.org), [Breathe](https://breathe.readthedocs.io/en/latest/) and [Exhale](https://exhale.readthedocs.io/en/latest/index.html) are required to compile the documentation. Once these are installed, just:

```bash
cd seq/doc
make html
```

You can then open `_build/html/index.html` with your browser.
