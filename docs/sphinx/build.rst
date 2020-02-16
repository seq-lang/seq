Building from Source
====================

Unless you really need to build Seq for whatever reason, we strongly
recommend using `pre-built binaries`_.

Really.

Dependencies
------------

-  Linux or macOS
-  `CMake`_ 3.10+
-  `LLVM`_ 6.0
-  `OCaml`_ 4.08
-  `Boehm GC`_ 7.6+
-  `HTSlib`_ 1.9+
-  `libffi`_ 3.2+
-  zlib
-  bz2
-  Python 3.6+
-  git

Platform-independent notes
^^^^^^^^^^^^^^^^^^^^^^^^^^

Ideally, you should build Seq with LLVM-Tapir 6.0.

However, there are no pre-compiled binaries for LLVM-Tapir 6.0, and you
need to build it yourself. In order to do so, please consult the
``before_install`` section of `our Travis script`_.

Building LLVM takes a lot of time (expect one hour at least). If you are
in a rush, just use a system-provided LLVM distribution. However, please
be advised that in that case:

1. multi-threading will not work, and
2. performance might be negatively affected for versions other than LLVM
   6.0 due to a `coroutine regression bug`_ (also discussed `here`_).

OPAM can also be problematic: if you get into any trouble with
OCaml/OPAM, get the latest version of OPAM through:

.. code-block:: bash

   sh <(curl -sL https://raw.githubusercontent.com/ocaml/opam/master/shell/install.sh)

macOS
^^^^^

Install the dependencies via:

.. code-block:: bash

   brew install cmake pkg-config llvm@6 opam libffi zlib bzip2 python git xz

Ubuntu/Debian
^^^^^^^^^^^^^

Install the dependencies via:

.. code-block:: bash

   apt install cmake pkg-config llvm-6.0 zlib1g-dev libbz2-dev libffi-dev python3 git liblzma-dev m4 unzip

To install OPAM, do

.. code-block:: bash

    sh <(curl -sL https://raw.githubusercontent.com/ocaml/opam/master/shell/install.sh)

CentOS/RHEL
^^^^^^^^^^^

Install the dependencies via:

.. code-block:: bash

    yum install cmake pkg-config llvm-toolset llvm-devel llvm-static zlib-devel bzip2-devel libffi-devel python3 git bubblewrap unzip xz-devel

To install OPAM, do

.. code-block:: bash

    sh <(curl -sL https://raw.githubusercontent.com/ocaml/opam/master/shell/install.sh)

You might need to enable the ``devtools`` repository to be able to install
LLVM. As instructions depend on your CentOS/RHEL version, please consult
the distribution manual.

OPAM setup
~~~~~~~~~~

To set up OPAM, do:

.. code-block:: bash

    opam init --bare
    opam switch install 4.08.1
    eval $(opam env)
    opam install core dune menhir ctypes ctypes-foreign ppx_deriving

If OPAM keeps complaining about missing ``bwrap`` or ``bubblewrap`` and your distribution does not contain such packages, run:

.. code-block:: bash

    opam init --bare --disable-sandboxing

Building
--------

First prepare the environment:

.. code-block:: bash

    mkdir seq/build
    cd seq/build

Then build the necessary dependencies:

.. code-block:: bash

    # Build bdw-gc
    curl -L https://www.hboehm.info/gc/gc_source/gc-8.0.4.tar.gz | tar zxvf -
    cd gc-8.0.4
    mkdir -p release
    ./configure --prefix=`pwd`/release --enable-threads=posix --enable-cplusplus --enable-thread-local-alloc --enable-large-config
    make LDFLAGS="-static"
    make install
    cd ..

    # Build HTSlib
    curl -L https://github.com/samtools/htslib/releases/download/1.9/htslib-1.9.tar.bz2 | tar jxvf -
    cd htslib-1.9
    ./configure CFLAGS="-fPIC" --disable-libcurl
    make
    cd ..

Then build Seq via:

.. code-block:: bash

    cmake .. -DLLVM_DIR=`llvm-config --cmakedir` -DHTS_LIB=htslib-1.9/libhts.a -DGC_LIB=gc-8.0.4/release/lib/libgc.a
    CPATH=gc-8.0.4/release/include:htslib-1.9 cmake --build .

This will produce a ``seqc`` executable for compiling/running Seq programs, and a ``seqtest`` executable for running the test suite.


Documentation
^^^^^^^^^^^^^

`Sphinx <https://www.sphinx-doc.org>`_ (with the `RTD theme <https://sphinx-rtd-theme.readthedocs.io/en/stable/>`_), `Breathe <https://breathe.readthedocs.io/en/latest/>`_ and `Exhale <https://exhale.readthedocs.io/en/latest/index.html>`_ are required to compile the documentation. Once these are installed, just:

.. code-block:: bash

    cd seq/docs/sphinx
    make html

You can then open ``_build/html/index.html`` with your browser.


.. _pre-built binaries: https://github.com/seq-lang/seq/releases
.. _CMake: https://cmake.org
.. _LLVM: https://llvm.org
.. _OCaml: https://ocaml.org
.. _Boehm GC: https://github.com/ivmai/bdwgc
.. _HTSlib: https://htslib.org
.. _libffi: https://sourceware.org/libffi
.. _our Travis script: https://github.com/seq-lang/seq/blob/master/.travis.yml
.. _coroutine regression bug: https://bugs.llvm.org/show_bug.cgi?id=40656
.. _here: https://www.reddit.com/r/cpp/comments/aoad7l/coroutine_allocation_elision_broken_in_clang_7
