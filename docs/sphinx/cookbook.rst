Cookbook
========

.. contents::

Subsequence extraction
----------------------

.. code-block:: seq

    myseq  = s'CAATAGAGACTAAGCATTAT'
    sublen = 5
    stride = 2

    # explicit for-loop:
    for subseq in myseq.split(sublen, stride):
        print subseq

    # pipelined:
    myseq |> split(sublen, stride) |> echo

:math:`k`-mer extraction
------------------------

.. code-block:: seq

    myseq  = s'CAATAGAGACTAAGCATTAT'
    type K = k5
    stride = 2

    # explicit for-loop:
    for subseq in myseq.kmers[k5](stride):
        print subseq

    # pipelined:
    myseq |> kmers[k5](stride) |> echo

