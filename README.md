# Seq — a language for computational genomics

[![Build Status](https://travis-ci.com/seq-lang/seq.svg?token=QGRVvAxcSasMm4MgJvYL&branch=master)](https://travis-ci.com/seq-lang/seq)
[![License](https://img.shields.io/badge/license-AGPL-blue.svg)](LICENSE)
[![Gitter](https://badges.gitter.im/Join%20Chat.svg)](https://gitter.im/seq-lang/seq?utm_source=badge&utm_medium=badge&utm_campaign=pr-badge&utm_content=badge)

## Dependencies

- Linux or macOS
- [LLVM](https://llvm.org)<sup>1</sup>
- [OCaml](https://ocaml.org)<sup>2</sup>
- [Boehm GC](https://github.com/ivmai/bdwgc)

<sup>1</sup> LLVM 6 or greater is required. However, due to a LLVM 7 [bug](https://bugs.llvm.org/show_bug.cgi?id=40656) with coroutines (which are used extensively in Seq), we highly recommend building with LLVM 6.

<sup>2</sup> The following packages must be installed with `opam`: ocamlfind, core, core_extended, core_bench, dune, ctypes, ctypes-foreign, ANSITerminal, menhir, ppx_deriving, zmq, nocrypto, yojson, cstruct, hex

## Build

Make sure the `LLVM_DIR` environment variable is set (to the result of `llvm-config --cmakedir`). Then:

```bash
mkdir seq/build
cd seq/builld
cmake ..
cmake --build .
```

This will produce a `seq` executable for compiling/running Seq programs, and a `seqtest` executable for running the test suite.


## Documentation

[Sphinx](http://www.sphinx-doc.org), [Breathe](https://breathe.readthedocs.io/en/latest/) and [Exhale](https://exhale.readthedocs.io/en/latest/index.html) are required to compile the documentation. Once these are installed, just:

```bash
cd seq/docs
make html
```

You can then open `_build/html/index.html` with your browser.
