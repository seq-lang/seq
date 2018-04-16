# Seq — a DSL for processing genomic data

## Dependencies

- C++11
- CMake 3.9+
- LLVM 5+

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

## At a glance

A full tutorial can be found in the docs, but here's a program showcasing some of the features:

```cpp
#include <iostream>
#include <seq/seq.h>

using namespace seq;
using namespace seq::types;
using namespace seq::stageutil;

SEQ_FUNC bool is_cpg_func(char *seq, seq_int_t len)
{
	return len >= 2 && seq[0] == 'C' && seq[1] == 'G';
}

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

int main()
{
	SeqModule s;

	Func is_cpg(Seq, Bool, SEQ_NATIVE(is_cpg_func));
	Func my_hash(Seq, Int, SEQ_NATIVE(my_hash_func));

	/*
	 * Multiple pipelines can be added
	 * to a source sequence
	 */
	s |
	split(10,1) |
	filter(is_cpg) |
	print() |
	substr(6,5) |
	copy() |
	revcomp() |
	split(1,1) |
	print();

	/*
	 * Vars can be used to refer back to
	 * results of previous pipelines
	 */
	Var kmers  = s | split(32,32) | collect();  // 'collect' collects inputs into an array
	Var hashes = kmers | foreach() | my_hash() | collect();
	hashes | foreach() | print();

	s | print() | copy() | revcomp() | print();

	Var a, b, c, d;
	a = s | print();
	b = a | substr(1,5);
	c = b | substr(1,1) | print();
	d = b | copy() | revcomp() | print();

	/*
	 * Pipelines can branch arbitrarily
	 */
	Pipeline x, y;
	x = s | split(32,1) | filter(is_cpg);
	x | print();
	y = x | substr(1,16);
	y | (print(),
	     copy() | revcomp() | print());  // convenient branch syntax

	/*
	 * Arrays can be declared
	 */
	Var m = s.once | Int[1000];  // array of 1000 integers
	                             // 's.once' is executed just once, at the start
	Var i, v;
	i = s | split(2,1) | filter(is_cpg) | count();
	i | print();
	s | split(1,1) | count() | m[i];
	i | m[i];
	v = m[i];
	v | print();
	m[i] | print();

	Var nums = s.last | Int[10];   // 's.last' is executed just once, at the end
	s.last | range(10) | nums[_];  // '_' refers to prev stage's output
	s.last | range(10) | nums[_] | print();

	/*
	 * Lambdas can be declared
	 */
	Lambda z;
	s.last | nums | foreach() | lambda(1 + z*2) | print();

	/*
	 * Functions can be declared
	 */
	Func f(Seq, Array.of(Seq));
	f | split(32,1) | filter(is_cpg) | collect();

	s.last | f() | foreach() | print();

	s.source("test/data/seqs.fastq");
	s.execute(true);  // debug=true
}
```
