## A DSL for processing genomic data

### Dependencies

- C++11
- CMake & Make
- LLVM 5+

### Build

Build with CMake:

```
cd seq
LLVM_DIR=/path/to/llvm/version/lib/cmake cmake .
make
```

Test:

```
./seqtest
```

### Tutorial

#### Basics

A `seq::Seq` instance essentially represents a single "module", which is a collection of pipelines
through which input sequences are passed. Here is a simple example which reads a FASTQ and prints the
reverse complements of each read's non-overlapping 32-mers:

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

Pipelines can branch arbitrarily.
For example, if we instead want to print each original 32-mer in 
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
    
s | split(32,32) | (print(), revcomp() | print());

s.source("input.fastq");
s.execute();
```

#### Stages

The following built-in stages are available. Each stage has an input and output type.

| Stage     | Types | Description |
| :-------- | :---- | :---------- |
| `copy()`  | `Seq` → `Seq` | Copies input sequence into a new buffer |
| `count()` | `Seq` → `Int` | Counts how many items are passed through the stage |
| `filter(fn)` | `Seq` → `Seq` | Filters sequences based on `fn` (which returns a boolean) |
| `hash(h)` | `Seq` → `Int` | Computes hash function `h` on input |
| `len()`   | `Seq` → `Int` | Returns length of input |
| `op(fn)` | `Seq` → `Seq` | Applies `fn` to the input sequence in place |
| `print()` | `Any` → `Any` | Prints and propagates the input |
| `range(from,to,step)` | `Void` → `Int` | Generates the specified range of integers |
| `revcomp()` | `Seq` → `Seq` | Reverse complements the input sequence in place |
| `split(k,step)` | `Seq` → `Seq` | Splits input into `k`-mers with step `step` |
| `substr(pos,len)` | `Seq` → `Seq` | Extracts the length-`len` substring of the input at `pos` |
| `foreach()` | `Array[T]` → `T` | Returns each element of input array in order |
| `collect()` | `T` → `Array[T]` | Collects all inputs into an array |
| `ser(file)` | `Any` → `Void` | Serializes input to `file` |
| `deser(T,file)` | `Void` → `T` | Deserializes type-`T` object from `file` |

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

Arrays can be declared like this:

```cpp
seq::Seq s;

Var m = s.once | Int[10];
...
```

`s.once` is for pipelines only to be executed once, such as declaring global memory.
As an example, we can construct a simple index of 8-mers for an input FASTA file:

```cpp
#include "seq.h"

using namespace seq;
using namespace seq::types;
using namespace seq::stageutil;

// simple 2-bit encoding
SEQ_FUNC seq_int_t my_hash_func(char *seq, seq_int_t len)
{
	seq_int_t h = 0;

	for (seq_int_t i = 0; i < len; i++) {
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
    const unsigned N = 1 << (2 * K);  // 4^K
    
    /* build the index */
    Var index = s.once | Int[N];
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
Var m = s.once | Int[N];

s.once | range(N) | m[_];
s.once | range(N) | m[_] | print();

s.source("input.fastq");
s.execute();
```

Note the following:
- `_` is a special variable that always refers to the output of the previous stage.
- We need the `s |` at the start of each pipeline so that they are associated with `s`. This is
sometimes implicit, as with variables (e.g. `myvar | ...`) and loads (e.g. `array[i] | ...`), but
with stages it must be explicit.

#### Lambdas

Here's an example of how lambdas can be used, which builds on the previous example:

```cpp
seq::Seq s;

const unsigned N = 10;
Var m = s.once | Int[N];
Lambda x;
s.once | range(N) | m[_];
s.once | range(N) | m[_] | lambda(2*x + 1) | print();

s.source("input.fastq");
s.execute();
```

This prints all the odd integers between 1 and 20. In general, `2*x + 1` can be any
arithmetic expression in terms of `x`. Note that we can also use `foreach` to iterate over arrays:

```cpp
s.once | m | foreach() | lambda(2*x + 1) | print();
```

#### Serialization

Objects can be serialized, and later deserialized either in the same program or in a
different one. For example, let's serialize and deserialize our 8-mer index above:

```cpp
seq::Seq s;

const unsigned K = 8;
const unsigned N = 1 << (2 * K);  // 4^K

/* build the index */
Var index = s.once | Int[N];
Pipeline kmers = s | split(K,1);
Var h = kmers | my_hash();
kmers | count() | index[h];  // store the kmer offset in our index

/* store & read the index */
s.last | index | ser("index.dat");
Var newIndex = s.last | deser(Array.of(Int), "index.dat");

s.source("input.fasta");
s.execute();
```

`s.last` is for pipelines to be executed once, after all input sequences are processed.
