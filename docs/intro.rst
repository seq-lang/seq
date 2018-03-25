Getting started
===============

Dependencies
------------

- C++11
- CMake & Make
- LLVM 5+

Compile & Test
--------------

In a nutshell:

.. code-block:: bash

    cd seq
    LLVM_DIR=/path/to/llvm/version/lib/cmake cmake .
    make

To run the test program:

.. code-block:: bash

    ./seqtest
