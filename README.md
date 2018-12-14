# Seq — a language for computational genomics

[![Build Status](https://travis-ci.com/seq-lang/seq.svg?token=QGRVvAxcSasMm4MgJvYL&branch=master)](https://travis-ci.com/seq-lang/seq)

## Dependencies

- C++11
- CMake 3.9+
- LLVM 5+
- Boehm GC
- OCaml

## Build

Compile:

```bash
cd seq/build
LLVM_DIR=/path/to/llvm/version/lib/cmake cmake ..
cmake --build .
```

Test:

```bash
./seqtest
```

## Documentation

[Sphinx](http://www.sphinx-doc.org) is used for all documentation. To compile, install Sphinx and then:

```bash
cd seq/docs
make html
```

You can then open `_build/html/index.html` with your browser.
