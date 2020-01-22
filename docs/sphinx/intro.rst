Getting Started
===============

Install
-------

Pre-built binaries
^^^^^^^^^^^^^^^^^^

Pre-built binaries for Linux and macOS on x86_64 are available alongside `each release <https://github.com/seq-lang/seq/releases>`_. We also have a script for downloading and installing pre-built versions:

.. code-block:: bash

    wget -O - https://raw.githubusercontent.com/seq-lang/seq/master/install.sh | bash

This will install Seq in a new ``.seq`` directory within your home directory. Be sure to update ``~/.bash_profile`` as the script indicates afterwards!

Seq binaries require a `libomp <https://openmp.llvm.org>`_ to be present on your machine. ``brew install libomp`` or ``apt install libomp5`` should do the trick.

Building from source
^^^^^^^^^^^^^^^^^^^^

See `Building from Source <build.html>`_.

Usage
-----

The ``seqc`` program can either directly run Seq source in JIT mode:

.. code-block:: bash

    seqc myprogram.seq

or produce an LLVM bitcode file if a ``-o <out.bc>`` argument is provided. In the latter case, `llc <https://llvm.org/docs/CommandGuide/llc.html>`_ and the system compiler can be used to convert the bitcode file to an object file and link it to produce an executable, respectively:

.. code-block:: bash

    seqc -o myprogram.bc myprogram.seq
    llc myprogram.bc -filetype=obj -o myprogram.o
    gcc -L/path/to/libseqrt/ -lseqrt -lomp -o myprogram myprogram.o

This produces a ``myprogram`` executable.

**Interfacing with C:** If a Seq program uses C functions from a particular library, that library can be specified via a ``-L/path/to/lib`` argument to ``seqc``. Otherwise it can be linked during the linking stage if producing an executable.
