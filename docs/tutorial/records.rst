Records
=======

Records can be thought of as tuples of multiple values. Record-creation syntax is straightforward; as an example, let's enumerate all 32-mers in our input sequence:

.. code-block:: c++

    SeqModule s;
    auto p = s | split(32,1) | (_, count());
    p | get(1) | print();
    p | get(2) | print();

    s.source("input.fastq");
    s.execute();

This will output something like this:

.. code-block:: text

    1
    GTCCTAAATTGTTGTACGAAAGAACGTGACAG
    2
    TCCTAAATTGTTGTACGAAAGAACGTGACAGA
    3
    CCTAAATTGTTGTACGAAAGAACGTGACAGAG
    4
    CTAAATTGTTGTACGAAAGAACGTGACAGAGG
    5
    TAAATTGTTGTACGAAAGAACGTGACAGAGGG
    ...

The record is created in ``s | split(32,1) | (_, count())``. Specifically, the input sequence is split into 32-mers, which are then stored into the ``(_, count())`` record. Recall that ``_`` is a special variable that always refers to the output of the previous stage (in this case, the 32-mers). ``count()``, on the other hand, is a simple incremental counter that gives us our enumeration.

The next two lines print the first and second elements of this record (i.e. the count and the 32-mer, respectively).

In general, records can be comprised of the outputs of any number of pipelines, as in ``(<pipeline 1>, <pipeline 2>, ..., <pipeline N>)``. They can also contain the values of variables (as we're actually doing above with ``_``), as in ``(<var 1>, <var 2>, ..., <var N>)``. These can be mixed and matched as well.

-----

:doc:`next <serialization>`
