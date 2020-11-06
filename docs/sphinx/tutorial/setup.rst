Set-up
======

Installation
------------

Simple!

.. code:: bash

    /bin/bash -c "$(curl -fsSL https://seq-lang.org/install.sh)"

Some features require `htslib <http://www.htslib.org/>`__ 1.10+ and
Python 3.5+. If you want to use Python interop, you also need to point
``SEQ_PYTHON`` to the Python library (typically called
``libpython3.8m.so`` or similar). Also, if you have a custom build of
htslib, you can point ``SEQ_HTSLIB`` to it
(e.g. ``export SEQ_HTSLIB=/path/to/libhts-custom.so``).

Usage
-----

Assuming that Seq was properly installed, you can use it as follows:

.. code:: bash

    seqc file.seq  # Compile and run file.seq
    seqc -d file.seq  # Compile and run file.seq in debug mode
    seqc -o file.bc file.seq  # Compile file.seq to LLVM bytecode file file.bc

It is highly recommended to use ``-d`` parameter for development
purposes: compilation is faster, stack traces are actually useful,
and it has some extra checks (e.g. null checks) that can save your life.

Creating a stand-alone executable
---------------------------------

Currently, stand-alone executables must be created manually:

.. code:: bash

    seqc -o prog.bc prog.seq
    llc prog.bc -filetype=obj -o prog.o
    clang -L/path/to/libseqrt/ -lseqrt -lomp -ldl -pthread -o prog prog.o

You might need to install LLVM to use ``llc``. Seq uses LLVM 6, so we
advise against using older versions. ``/path/to/libseqrt/`` would typically
be ``$HOME/.seq/lib/seq``.

If you want to be able to easily distribute your executable, pass
``-Wl,-rpath,\$ORIGIN`` to ``clang`` and ship the ``libseqrt.so`` and
``libomp.so`` in the same directory with your executable. This is a
Linux-specific argument; on macOS you might want to use
``-Wl,-rpath,"@loader_path"`` instead.
