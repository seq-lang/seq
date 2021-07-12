Building from Source
====================

Unless you really need to build Seq for whatever reason, we strongly
recommend using pre-built binaries.

Dependencies
------------

We have a script ``scripts/deps.sh`` for downloading and building all
of Seq's dependencies:

.. code-block:: bash

    ./scripts/deps.sh 2

This will download all dependencies to a new ``deps_src`` folder and
compile them to a new ``deps`` folder (both in the current directory)
using 2 cores.

This script works on macOS and Ubuntu; it might need to be modified
to work on other platforms.

Compiling all dependencies can take upwards of an hour on a single core,
but it only needs to be done once.

Build
-----

The following can generally be used to build Seq once the dependencies
are compiled:

.. code-block:: bash

    mkdir build
    (cd build && cmake .. -DCMAKE_BUILD_TYPE=Release \
                          -DSEQ_DEP=/path/to/deps \
                          -DCMAKE_C_COMPILER=clang \
                          -DCMAKE_CXX_COMPILER=clang++)
    cmake --build build --config Release

This should produce the ``seqc`` executable in the ``build`` directory, as well as
``seqtest`` which runs the test suite.
