## A DSL for processing genomic data

### Dependencies

- C++11
- CMake / Make
- LLVM 5+

### Build

Build with CMake:

```
cd seq
LLVM_DIR=/usr/local/Cellar/llvm/5.0.1/lib/cmake cmake .
make
```

where LLVM_DIR is ... well, LLVM_DIR.

Test:

```
./seqtest
```

