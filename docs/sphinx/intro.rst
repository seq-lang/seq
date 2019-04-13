Getting Started
===============

Dependencies
------------

- Linux or macOS
- `LLVM <https://llvm.org>`_ [#]_
- `OCaml <https://ocaml.org>`_ [#]_
- `Boehm GC <https://github.com/ivmai/bdwgc>`_

.. [#] LLVM 6 or greater is required. However, due to a LLVM 7 `bug <https://bugs.llvm.org/show_bug.cgi?id=40656>`_ with coroutines (which are used extensively in Seq), we highly recommend building with LLVM 6.

.. [#] The following packages must be installed with ``opam``: ocamlfind, core, core_extended, core_bench, dune, ctypes, ctypes-foreign, ANSITerminal, menhir, ppx_deriving, zmq, nocrypto, yojson, cstruct, hex

Compile & Test
--------------

Make sure the ``LLVM_DIR`` environment variable is set (to the result of ``llvm-config --cmakedir``). Then:

.. code-block:: bash

    mkdir seq/build
    cd seq/builld
    cmake ..
    cmake --build .

This will produce a ``seq`` executable for compiling/running Seq progrms, and a ``seqtest`` executable for running the test suite.

Usage
-----

The ``seq`` program can either directly run Seq source in JIT mode, or produce an LLVM bitcode file if a ``-o <out.bc>`` argument is provided. In the latter case, `llc <https://llvm.org/docs/CommandGuide/llc.html>`_ and the system compiler can be used to convert the bitcode file to an object file and link it to produce an executable, respectively:

.. code-block:: bash

    seq -o myprogram.bc myprogram.seq
    llc myprogram.bc -filetype=obj -o myprogram.o
    g++ -L/path/to/libseqrt/ -lseqrt -o myprogram myprogram.o

This produces a ``myprogram`` executable. (If multithreading is needed, the ``g++`` invocation should also include ``-fopenmp``.)

**Interfacing with C:** If a Seq program uses C functions from a particular library, that library can be specified via a ``-L/path/to/lib`` argument to the ``seq`` program. Otherwise it can be linked during the linking stage if producing an executable.
