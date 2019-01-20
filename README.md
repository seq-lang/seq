# Seq — a language for computational genomics

[![Build Status](https://travis-ci.com/seq-lang/seq.svg?token=QGRVvAxcSasMm4MgJvYL&branch=master)](https://travis-ci.com/seq-lang/seq)
[![Gitter](https://badges.gitter.im/Join%20Chat.svg)](https://gitter.im/seq-lang/seq)

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

[Sphinx](http://www.sphinx-doc.org), [Breathe](https://breathe.readthedocs.io/en/latest/) and [Exhale](https://exhale.readthedocs.io/en/latest/index.html) are required to compile the documentation. Once these are installed, just:

```bash
cd seq/docs
make html
```

You can then open `_build/html/index.html` with your browser.
