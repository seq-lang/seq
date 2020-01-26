**Seq** — a language for bioinformatics
=======================================

Seq is a programming language for computational genomics and bioinformatics. With a Python-compatible syntax and a host of domain-specific features and optimizations, Seq makes writing high-performance genomics software as easy as writing Python code, and achieves performance comparable to (and in many cases better than) C/C++.

Source code is available `on GitHub <https://github.com/seq-lang/seq>`_. You can also learn more about Seq from `our paper <https://dl.acm.org/citation.cfm?id=3360551>`_ or by visiting our `Gitter chatroom <https://gitter.im/seq-lang/Seq?utm_source=share-link&utm_medium=link&utm_campaign=share-link>`_.

.. toctree::
   :maxdepth: 1

   intro
   tutorial
   cookbook
   python
   embed
   build
   internals
   api/doxygen

Frequently Asked Questions
--------------------------

    *What is the goal of Seq?*

One of the main focuses of Seq is to bridge the gap between usability and performance in the fields of bioinformatics and computational genomics, which have an unfortunate reputation for hard-to-use, buggy or generally poorly-written software. Seq aims to make writing high-performance genomics or bioinformatics software substantially easier, and to provide a common, unified framework for the development of such software.

    *Why do we need a whole new language? Why not a library?*

There are many great bioinformatics libraries on the market today, including `Biopython <https://biopython.org>`_ for Python, `SeqAn <https://www.seqan.de>`_ for C++ and `BioJulia <https://biojulia.net>`_ for Julia. In fact, Seq offers a lot of the same functionality found in these libraries. The advantages of having a domain-specific language and compiler, however, are the higher-level constructs and optimizations like :ref:`pipeline`, :ref:`match`, :ref:`interalign` and :ref:`prefetch`, which are difficult to replicate in a library, as they often involve large-scale program transformations/optimizations. A domain-specific language also allows us to explore different backends like GPU, TPU or FPGA in a systematic way, in conjunction with these various constructs/optimizations, which is ongoing work.

    *What about interoperability with other languages and frameworks?*

Interoperability is and will continue to be a priority for the Seq project. We don't want using Seq to render you unable to use all the other great frameworks and libraries that exist. Seq already supports interoperability with C/C++ and Python (see :ref:`interop`), which we are in the process of expanding (e.g. by allowing Python libraries to be written in Seq).

   *I want to contribute! How do I get started?*

Great! Check out our `contribution guidelines <https://github.com/seq-lang/seq/blob/master/CONTRIBUTING.md>`_ and `open issues <https://github.com/seq-lang/seq/issues>`_ to get started. Also don't hesitate to drop by our `Gitter chatroom <https://gitter.im/seq-lang/Seq?utm_source=share-link&utm_medium=link&utm_campaign=share-link>`_ if you have any questions.

   *What is planned for the future?*

See the `roadmap <https://github.com/seq-lang/seq/wiki/Roadmap>`_ for information about this.
