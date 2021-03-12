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

    seqc foo.seq  # Compile and run foo.seq
    seqc -release foo.seq  # Compile and run foo.seq with optimizations
    seqc -build -exe file.seq  # Compile foo.seq executable "foo"

Note that the ``-exe`` option requires ``clang`` to be installed, and
the ``LIBRARY_PATH`` environment variable to point to the Seq runtime
library (installed by default at ``~/.seq/libs/``).
