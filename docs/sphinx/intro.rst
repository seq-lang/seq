Getting Started
===============

Source code is available `on GitHub <https://github.com/seq-lang/seq>`_.

Dependencies
------------

- Linux or macOS
- `CMake <https://cmake.org>`_ 3.12+
- `LLVM <https://llvm.org>`_ 6.0
- `OCaml <https://ocaml.org>`_ 4.08
- `Boehm GC <https://github.com/ivmai/bdwgc>`_ 7.6+
- `HTSlib <https://htslib.org>`_ 1.9+
- `libffi <https://sourceware.org/libffi>`_ 3.2+

The following packages must be installed with ``opam``: core, ctypes, ctypes-foreign, menhir, ppx_deriving

Compile & Test
--------------

Make sure the ``LLVM_DIR`` environment variable is set (to the result of ``llvm-config --cmakedir``). Then:

.. code-block:: bash

    mkdir seq/build
    cd seq/builld
    cmake ..
    cmake --build .

This will produce a ``seqc`` executable for compiling/running Seq progrms, and a ``seqtest`` executable for running the test suite.

Usage
-----

The ``seqc`` program can either directly run Seq source in JIT mode, or produce an LLVM bitcode file if a ``-o <out.bc>`` argument is provided. In the latter case, `llc <https://llvm.org/docs/CommandGuide/llc.html>`_ and the system compiler can be used to convert the bitcode file to an object file and link it to produce an executable, respectively:

.. code-block:: bash

    seqc -o myprogram.bc myprogram.seq
    llc myprogram.bc -filetype=obj -o myprogram.o
    g++ -L/path/to/libseqrt/ -lseqrt -o myprogram myprogram.o

This produces a ``myprogram`` executable. (If multithreading is needed, the ``g++`` invocation should also include ``-fopenmp``.)

**Interfacing with C:** If a Seq program uses C functions from a particular library, that library can be specified via a ``-L/path/to/lib`` argument to ``seqc``. Otherwise it can be linked during the linking stage if producing an executable.
