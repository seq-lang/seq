<p align="center">
 <img src="https://raw.githubusercontent.com/seq-lang/seq/master/docs/images/logo.png?token=AASLWCFONKC7XVZRJ3KAABS5NAU4Q" width="200" alt="Seq"/>
</p>

<h1 align="center"> Seq — a language for bioinformatics </h1>

<p align="center">
  <a href="https://travis-ci.com/seq-lang/seq">
    <img src="https://travis-ci.com/seq-lang/seq.svg?token=QGRVvAxcSasMm4MgJvYL&branch=master"
         alt="Build Status">
  </a>
  <a href="https://github.com/seq-lang/seq/blob/master/LICENSE">
    <img src="https://img.shields.io/badge/license-AGPL-blue.svg"
         alt="License">
  </a>
  <a href="https://gitter.im/seq-lang/seq?utm_source=badge&utm_medium=badge&utm_campaign=pr-badge&utm_content=badge">
    <img src="https://badges.gitter.im/Join%20Chat.svg"
         alt="Gitter">
  </a>
</p>

## Introduction

Seq is a programming language for computational genomics and bioinformatics. With a Python-compatible syntax and a host of domain-specific features and optimizations, Seq makes writing high-performance genomics software as easy as writing Python code, and achieves performance comparable to (and in many cases better than) C/C++.

Learn more by following the [tutorial](docs/sphinx/tutorial.rst) or from the [cookbook](docs/sphinx/cookbook.rst).

## Example

Here is an example seeding application in Seq using a hypothetical genome index, like what is typically found in seed-and-extend alignment algorithms:

```python
from sys import argv
from genomeindex import *
type K = Kmer[20]

# index and process 20-mers
def process(kmer: K,
            index: GenomeIndex[K]):
    prefetch index[kmer], index[~kmer]
    hits_fwd = index[kmer]
    hits_rev = index[~kmer]
    ...

# index over 20-mers
index = GenomeIndex[K](argv[1])

# stride for k-merization
stride = 10

# sequence-processing pipeline
(fastq(argv[2])
  |> kmers[K](stride)
  |> process(index))
```

A few notable aspects of this code:

- Seq provides native k-mer types, e.g. a 20-mer is represented by `Kmer[20]` as above.
- k-mers can be reverse-complemented with `~`.
- Seq provides easy iteration over common formats like FASTQ (`fastq` above).
- Complex pipelines are easily expressible in Seq (via the `|>` syntax).
- Seq can perform pipeline transformations to make genomic index lookups faster via `prefetch`.

For a concrete example of `genomeindex`, check out our [re-implementation of SNAP's index](test/snap).

## Dependencies

- Linux or macOS
- [LLVM](https://llvm.org)<sup>1</sup>
- [OCaml](https://ocaml.org)<sup>2</sup>
- [Boehm GC](https://github.com/ivmai/bdwgc)

<sup>1</sup> LLVM 6 or greater is required. However, due to a LLVM 7 [bug](https://bugs.llvm.org/show_bug.cgi?id=40656) with coroutines (which are used extensively in Seq), we highly recommend building with LLVM 6.

<sup>2</sup> The following packages must be installed with `opam`: core, dune, ctypes, ctypes-foreign, menhir, ppx_deriving

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

[Sphinx](https://www.sphinx-doc.org) (with the [RTD theme](https://sphinx-rtd-theme.readthedocs.io/en/stable/)), [Breathe](https://breathe.readthedocs.io/en/latest/) and [Exhale](https://exhale.readthedocs.io/en/latest/index.html) are required to compile the documentation. Once these are installed, just:

```bash
cd seq/docs/sphinx
make html
```

You can then open `_build/html/index.html` with your browser.
