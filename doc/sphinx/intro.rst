Getting Started
===============

Dependencies
------------

- C++11
- CMake 3.9+
- LLVM 5+
- Boehm GC
- OCaml

Compile & Test
--------------

Make sure the ``LLVM_DIR`` environment variable is set (to the result of ``llvm-config --cmakedir``). Then:

.. code-block:: bash

    mkdir seq/build
    cd seq/builld
    cmake ..
    cmake --build .

This will produce a ``seq`` executable for compiling/running Seq progrms, and a ``seqtest`` executable for running the test suite.

.. code-block:: bash

    ./seqtest
