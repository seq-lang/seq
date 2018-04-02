## Seq — a DSL for processing genomic data

### Dependencies

- C++11
- CMake & Make
- LLVM 5+

### Build

Compile:

```bash
cd seq
LLVM_DIR=/path/to/llvm/version/lib/cmake cmake .
make
```

Test:

```bash
./seqtest
```

### Documentation

[Sphinx](http://www.sphinx-doc.org) is used for all documentation. To compile, install Sphinx and then:

```bash
cd seq/docs
make html
```

You can then open `_build/html/index.html` with your browser.

### At a glance

Here's an example program for printing the reverse complements of all non-overlapping 32-mers from an input FASTQ:

```cpp
#include "seq.h"

using namespace seq;
using namespace seq::stageutil;

int main()
{
    seq::Seq s;
    
    s | split(32,32) | revcomp() | print();

    s.source("input.fastq");
    s.execute();
}
```

This compiles to the following LLVM IR:

```llvm
pipeline:                                         ; preds = %entry
  br label %loop

loop:                                             ; preds = %loop, %pipeline
  %i = phi i32 [ 0, %pipeline ], [ %next, %loop ]
  %1 = getelementptr i8, i8* %seq, i32 %i
  call void @revcomp(i8* %1, i32 32)
  call void @print(i8* %1, i32 32)
  %next = add i32 %i, 32
  %2 = sub i32 %len, 32
  %3 = icmp ule i32 %next, %2
  br i1 %3, label %loop, label %exit
```

A full tutorial can be found in the docs. 
