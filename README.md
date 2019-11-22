<p align="center">
 <img src="docs/images/logo.png?raw=true" width="200" alt="Seq"/>
</p>

<h1 align="center"> Seq — a  language for bioinformatics </h1>

<p align="center">
  <a href="https://travis-ci.com/seq-lang/seq">
    <img src="https://travis-ci.com/seq-lang/seq.svg?branch=master"
         alt="Build Status">
  </a>
  <a href="https://gitter.im/seq-lang/seq?utm_source=badge&utm_medium=badge&utm_campaign=pr-badge&utm_content=badge">
    <img src="https://badges.gitter.im/Join%20Chat.svg"
         alt="Gitter">
  </a>
  <a href="https://github.com/seq-lang/seq/releases/latest">
    <img src="https://img.shields.io/github/v/release/seq-lang/seq?sort=semver"
         alt="Version">
  </a>
  <a href="https://github.com/seq-lang/seq/blob/master/LICENSE">
    <img src="https://img.shields.io/github/license/seq-lang/seq"
         alt="License">
  </a>
</p>

## Introduction

Seq is a programming language for computational genomics and bioinformatics. With a Python-compatible syntax and a host of domain-specific features and optimizations, Seq makes writing high-performance genomics software as easy as writing Python code, and achieves performance comparable to (and in many cases better than) C/C++.

Think of Seq as a strongly-typed and statically-compiled Python: all the bells and whistles of Python, boosted with strong type system, without any performance overhead.

Seq is able to outperform a Python code up to 160x. Seq can further beat equivalent C/C++ code up to 2x without any manual interventions. Seq also natively supports parallelism out of the box. Implementation details and benchmarks are discussed [in our paper](https://dl.acm.org/citation.cfm?id=3360551).

Learn more by following the [tutorial](docs/sphinx/tutorial.rst) or from the [cookbook](docs/sphinx/cookbook.rst).

## Example

Seq is a Python-compatible language, where vast majority of Python programs should work without any issues:

```python
def check_prime(n):
    if n > 1:
        for i in range(2, n):
            if n % i == 0:
                return False
        return True
    else: 
        return False
       
n = 1009
print n, 'is', 'a' if check_prime(n) else 'not a', 'prime'
```

Here is an example that showcases Seq's bioinformatics features: a seeding application in Seq using a hypothetical genome index, like what is typically found in seed-and-extend alignment algorithms:

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
(FASTQ(argv[2])
  |> seqs
  |> kmers[K](stride)
  |> process(index))
```

A few notable aspects of this code:

- Seq provides native k-mer types, e.g. a 20-mer is represented by `Kmer[20]` as above.
- k-mers can be reverse-complemented with `~`.
- Seq provides easy iteration over common formats like FASTQ (`FASTQ` above).
- Complex pipelines are easily expressible in Seq (via the `|>` syntax).
- Seq can perform pipeline transformations to make genomic index lookups faster via `prefetch`.

For a concrete example of `genomeindex`, check out our [re-implementation of SNAP's index](test/snap).

Learn more by following the [tutorial](docs/sphinx/tutorial.rst) or from the [cookbook](docs/sphinx/cookbook.rst).

## Install

### Pre-built binaries

Pre-built binaries for Linux and macOS on x86_64 are available alongside [each release](https://github.com/seq-lang/seq/releases). We also have a script for downloading and installing pre-built versions:

```bash
wget -O - https://raw.githubusercontent.com/seq-lang/seq/master/install.sh | bash
```

This will install Seq in a new ``.seq`` directory within your home directory. Be sure to update ``~/.bash_profile`` as the script indicates afterwards!

### Build from source

See [Building from Source](docs/sphinx/build.rst).

## Documentation 

Please check [seq-lang.org](https://seq-lang.org) for in-depth documentation.

## Citing Seq

If you use Seq in your research, please cite:

> Ariya Shajii, Ibrahim Numanagić, Riyadh Baghdadi, Bonnie Berger, and Saman Amarasinghe. 2019. Seq: a high-performance language for bioinformatics. *Proc. ACM Program. Lang.* 3, OOPSLA, Article 125 (October 2019), 29 pages. DOI: https://doi.org/10.1145/3360551

BibTeX:

```
@article{Shajii:2019:SHL:3366395.3360551,
 author = {Shajii, Ariya and Numanagi\'{c}, Ibrahim and Baghdadi, Riyadh and Berger, Bonnie and Amarasinghe, Saman},
 title = {Seq: A High-performance Language for Bioinformatics},
 journal = {Proc. ACM Program. Lang.},
 issue_date = {October 2019},
 volume = {3},
 number = {OOPSLA},
 month = oct,
 year = {2019},
 issn = {2475-1421},
 pages = {125:1--125:29},
 articleno = {125},
 numpages = {29},
 url = {http://doi.acm.org/10.1145/3360551},
 doi = {10.1145/3360551},
 acmid = {3360551},
 publisher = {ACM},
 address = {New York, NY, USA},
 keywords = {Python, bioinformatics, computational biology, domain-specific language, optimization, programming language},
}
```
