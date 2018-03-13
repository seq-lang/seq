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

### Tutorial

#### Basics

A `seq::Seq` instance essentially represents a single "module", which is a collection of pipelines
through which input sequences are passed. Here is a simple example which reads a FASTQ and prints the
reverse complements of each read's nonoverlapping 32-mers:

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

Pipelines can branch arbitrarily. For example, if we instead want to print each original 32-mer in 
addition to the reverse complements (boilerplate is omitted for brevity):

```cpp
seq::Seq s;
    
Pipeline kmers = s | split(32,32);
kmers | print();
kmers | revcomp() | print();

s.source("input.fastq");
s.execute();
```

Branching pipelines can also be represented using a `,` shorthand:

```cpp
seq::Seq s;
    
s | split(32,32) | (print(),
                    revcomp() | print());

s.source("input.fastq");
s.execute();
```

#### Stages

The following built-in stages are available. Each stage has an input and output type.

| Stage     | Types | Description |
| :-------- | :---- | :---------- |
| `copy()`  | `Seq -> Seq` | Copies input sequence into a new buffer |
| `count()` | `Seq -> Int` | Counts how many items are passed through the stage |
| `filter(fn)` | `Seq -> Seq` | Filters sequences based on `fn` (which returns a boolean) |
| `hash(h)` | `Seq -> Int` | Computes hash function `h` on input |
| `len()`   | `Seq -> Int` | Returns length of input |
| `op(fn)` | `Seq -> Seq` | Applies `fn` to the input sequence in place |
| `print()` | `Any -> Any` | Prints the input |
| `range(from,to,step)` | `Void -> Int` | Generates the specified range of integers |
| `revcomp()` | `Seq -> Seq` | Reverse complements the input sequence in place |
| `split(k,step)` | `Seq -> Seq` | Splits input into `k`-mers with step `step` |
| `substr(pos,len)` | `Seq -> Seq` | Extracts the length-`len` substring of the input at `pos` |

#### Variables

Variables can be used to refer to intermediate values in a pipeline. For example:

```cpp
seq::Seq s;

Var a = s | copy();
Var b = a | len();
Var c = b | substr(1,1);
c | print();
Var d = b | copy() | revcomp();
d | print();

s.source("input.fastq");
s.execute();
```

#### Arrays

Arrays can be declared using the `mem()` function, which is parameterized by the base type and array
size, as such:

```cpp
seq::Seq s;

const unsigned N = 10;
Mem m = s.mem<Int>(N);
...
```

As an example, we can construct a simple index of 8-mers for an input FASTA file:

```cpp
#include "seq.h"

using namespace seq;
using namespace seq::types;
using namespace seq::stageutil;

// simple 2-bit encoding
extern "C" uint32_t my_hash_func(char *seq, uint32_t len)
{
	uint32_t h = 0;

	for (uint32_t i = 0; i < len; i++) {
		h <<= 2;

		switch (seq[i]) {
			case 'A':
			case 'a':
				h += 0;
				break;
			case 'C':
			case 'c':
				h += 1;
				break;
			case 'G':
			case 'g':
				h += 2;
				break;
			case 'T':
			case 't':
				h += 3;
				break;
			default:
				break;
		}
	}
	return h;
}

static Hash& my_hash()
{
	return hash("my_hash_func", my_hash_func);
}

int main()
{
    seq::Seq s;

    const unsigned K = 8;
    Mem index = s.mem<Int>(1 << (2 * K));  // 4^K
    Pipeline kmers = s | split(K,1);

    Var h = kmers | my_hash();
    kmers | count() | index[h];  // store the kmer offset in our index

    s.source("input.fasta");
    s.execute();
}
```

As a second example, let's store the integers from 0 to 9 in an array and print them
(again, boilerplate omitted for brevity):

```cpp
seq::Seq s;

const unsigned N = 10;
Mem m = s.mem<Int>(N);

s | range(N) | m[_];
s | range(N) | m[_] | print();

s.source("input.fastq");
s.execute();
```

Note the following:
- `_` is a special variable that always refers to the output of the previous stage.
- We need the `s |` at the start of each pipeline so that they are associated with `s`. This is
sometimes implicit, as with variables (e.g. `myvar | ...`) and loads (e.g. `array[i] | ...`), but
with stages it must be explicit.
