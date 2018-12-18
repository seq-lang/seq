# Seq — a language for computational genomics

[![Build Status](https://travis-ci.com/seq-lang/seq.svg?token=QGRVvAxcSasMm4MgJvYL&branch=master)](https://travis-ci.com/seq-lang/seq)


## Dependencies

- C++11
- CMake 3.9+
- LLVM 5+
- Boehm GC
- OCaml


## Build

Make sure the `LLVM_DIR` environment variable is set (to the result of `llvm-config --cmakedir`). Then:

```bash
mkdir seq/build
cd seq/builld
cmake ..
cmake --build .
```

This will produce a `seq` executable for compiling/running Seq progrms, and a `seqtest` executable for running the test suite.


## Documentation

[Sphinx](http://www.sphinx-doc.org) is used for all documentation. To compile, install Sphinx and then:

```bash
cd seq/doc
make html
```

You can then open `_build/html/index.html` with your browser.
