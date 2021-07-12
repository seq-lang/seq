<p align="center">
 <img src="docs/sphinx/logo.png?raw=true" width="200" alt="Seq"/>
</p>

<h1 align="center"> Seq — a language for bioinformatics</h1>

<p align="center">
  <a href="https://github.com/seq-lang/seq/actions?query=branch%3Adevelop">
    <img src="https://github.com/seq-lang/seq/workflows/Seq%20CI/badge.svg?branch=develop"
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

> **A strongly-typed and statically-compiled high-performance Pythonic language!**

Seq is a programming language for computational genomics and bioinformatics. With a Python-compatible syntax and a host of domain-specific features and optimizations, Seq makes writing high-performance genomics software as easy as writing Python code, and achieves performance comparable to (and in many cases better than) C/C++.

**Think of Seq as a strongly-typed and statically-compiled Python: all the bells and whistles of Python, boosted with a strong type system, without any performance overhead.**

Seq is able to outperform Python code by up to 160x. Seq can further beat equivalent C/C++ code by up to 2x without any manual interventions, and also natively supports parallelism out of the box. Implementation details and benchmarks are discussed [in our paper](https://dl.acm.org/citation.cfm?id=3360551).

Learn more by following the [tutorial](https://docs.seq-lang.org/tutorial) or from the [cookbook](https://docs.seq-lang.org/cookbook).

## Examples

Seq is a Python-compatible language, and the vast majority of Python programs should work without any modifications:

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

Here is an example showcasing Seq's bioinformatics features:

```python
s = s'ACGTACGT'    # sequence literal
print s[2:5]       # subsequence
print ~s           # reverse complement
kmer = Kmer[8](s)  # convert to k-mer
K2 = Kmer[2]       # type definition

# iterate over length-3 subsequences
# with step 2
for sub in s.split(3, step=2):
    print sub[-1]  # last base

    # iterate over 2-mers with step 1
    for kmer in sub.kmers[K2](step=1):
        print ~kmer  # '~' also works on k-mers
```

Seq provides native sequence and k-mer types, e.g. a 8-mer is represented by `Kmer[8]` as above.

Here is a more complex example that counts occurrences of subsequences from a FASTQ file (`argv[2]`) in sequences obtained from a FASTA file (`argv[1]`) using an FM-index:

```python
from sys import argv
from bio.fmindex import FMIndex
fmi = FMIndex(argv[1])
k, step, n = 20, 20, 0

def add(count: int):
    global n
    n += count

@prefetch
def search(s: seq, fmi: FMIndex):
    intv = fmi.interval(s[-1])
    s = s[:-1]  # trim last base
    while s and intv:
        # backwards-extend intv
        intv = fmi[intv, s[-1]]
        s = s[:-1]  # trim last
    # return count of occurrences
    return len(intv)

FASTQ(argv[2]) |> seqs |> split(k, step) |> search(fmi) |> add
print 'total:', n
```

The `@prefetch` annotation tells the compiler to perform a coroutine-based pipeline transformation to make the FM-index queries faster, by overlapping the cache miss latency from one query with other useful work. In practice, the single `@prefetch` line can provide a 2x performance improvement.

## Install

### Pre-built binaries

Pre-built binaries for Linux and macOS on x86_64 are available alongside [each release](https://github.com/seq-lang/seq/releases). We also have a script for downloading and installing pre-built versions:

```bash
/bin/bash -c "$(curl -fsSL https://seq-lang.org/install.sh)"
```

### Build from source

See [Building from Source](docs/sphinx/build.rst).

## Documentation

Please check [docs.seq-lang.org](https://docs.seq-lang.org) for in-depth documentation.

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
