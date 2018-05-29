# Seq — a DSL for processing genomic data

## Dependencies

```bash
make install-osx
# restart shell
opam switch 4.06.1
make install-dep
```

## Build

Compile:

```bash
make
```

Test:

```bash
echo "\!load \"seqs.fasta\" |split 32 32 | print" | ./main.byte
```

Prepend line with `!` to execute it with JIT; otherwise you will just get IR.

## At a glance

```bash
# version 1 (works)
load "test.fq" | split 32 32 | print

# version 2 (nope)
let v = "ACGT\nAACC" |> split 32 32
v |> print

# version 3
let revcomp x =
	if x == "A" then "T" else 
		(if x == "G" then "C" else 
			(if x == "C" then G else 
				(if x == "T" then A else x)))
let i = load "test.fq" 
let a = split 32 32
let b = revcomp |> split 32 32
i |> a <&> b |> print
```
