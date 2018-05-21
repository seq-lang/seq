# Seq — a DSL for processing genomic data

## Dependencies

```bash
brew install ocaml opam llvm
opam init
# restart shell
opam switch 4.06.1
opam install async yojson core_extended core_bench \  
	cohttp async_graphics cryptokit menhir \
	llvm camlp4 batteries ctypes-foreign ppx_deriving
```

## Build

Compile:

```bash
corebuild main.byte
```

Test:

```bash
./main.byte
```

## At a glance

Still TODO. The target goal:

```bash
# version 1
load "test.fq" |> split 32 32 |> print

# version 2
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
