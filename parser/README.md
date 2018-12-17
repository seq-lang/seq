# Seq/OCaml: lexing and parsing for Seq language

## Dependencies

- Linux or macOS
- OCaml 4.06+ and OPAM
- OCaml dependencies:
  - Jane Street Core and PPXes
  - Dune
  - Odoc
  - Ctypes
  - ANSITerminal
  - Menhir
  - Install via
      ```
      opam install core core_extended dune menhir ctypes ctypes.foreign ansiterminal odoc
      ```

## Building

Just type `make` and you should be set!

OCaml bindings expect that C++ library is already built. If it is not and if your C++ build directory is `../build` (most likely it is), you should first run:
```
make -C ../build
```

## Running

First set up necessary environment variables:
```
export LD_LIBRARY_PATH=$(pwd)
export SEQ_PATH=../stdlib
```

Then:
```
./main.exe <seq.file> <args...>
```

## Docs

All OCaml functions should be documented within corresponding `ml`/`mli` files.

[GOTCHAS & Notes](notes.md)