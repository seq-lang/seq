# Seq/OCaml: lexing and parsing for Seq language

## Dependencies

- Linux or macOS
- OCaml 4.06+ and OPAM
- OCaml dependencies:
  - Jane Street Core and PPXes
  - Dune
  - Ctypes
  - Menhir
  - Install via
      ```
      opam install core dune menhir ctypes ctypes.foreign
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

## Jupyter

To add kernel do:

```
mkdir -p ~/.ipython/kernels/seq
cat > ~/.ipython/kernels/seq/kernel.json <<<EOF
{
    "argv": ["./main.exe", "--jupyter", "{connection_file}"],
    "display_name": "Seq",
    "language": "Seq"
}
EOF
```

Then run `jupyter notebook .` from the directory that contains `main.exe`.

> TODO
> `jupyter console --kernel seq` is bit flaky right now--- requires `C-c` after each command (probably needs some ZMQ reply).

## Docs

All OCaml functions should be documented within corresponding `ml`/`mli` files.

[GOTCHAS & Notes](notes.md)
