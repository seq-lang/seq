Introduction
============

A ``seq::SeqModule`` instance essentially represents a single "module", which is a collection of pipelines through which input sequences are passed. Here is a simple example which reads a FASTQ and prints the reverse complements of each read's non-overlapping 32-mers:

.. code-block:: c++

    #include <seq/seq.h>

    using namespace seq;
    using namespace seq::stageutil;

    int main()
    {
        SeqModule s;

        s | split(32,32) | revcomp() | print();

        s.source("input.fastq");
        s.execute();
    }


This compiles to the following LLVM IR:

.. code-block:: llvm

    pipeline:                                         ; preds = %entry
      br label %loop

    loop:                                             ; preds = %loop, %pipeline
      %i = phi i32 [ 0, %pipeline ], [ %next, %loop ]
      %1 = getelementptr i8, i8* %seq, i32 %i
      call void @revcomp(i8* %1, i32 32)
      call void @print(i8* %1, i32 32)
      %next = add i32 %i, 32
      %2 = sub i32 %len, 32
      %3 = icmp ule i32 %next, %2
      br i1 %3, label %loop, label %exit

Several input formats are supported, including TXT, FASTA and FASTQ. Often, multiple input files are associated with one another and need to be processed simultaneously (e.g. paired-end FASTQs). This is also supported:

.. code-block:: c++

    s[1] | ...  // mate 1
    s[2] | ...  // mate 2
    ...
    s.source("input_1.fastq", "input_2.fastq");

Pipelines can branch arbitrarily. For example, if we instead want to print each original 32-mer in addition to the reverse complements (boilerplate is omitted for brevity from here on out):

.. code-block:: cpp

    SeqModule s;

    Pipeline kmers = s | split(32,32);
    kmers | print();
    kmers | revcomp() | print();

    s.source("input.fastq");
    s.execute();

Branching pipelines can also be represented using a `,` shorthand:

.. code-block:: cpp

    SeqModule s;

    s | split(32,32) | (print(), revcomp() | print());

    s.source("input.fastq");
    s.execute();

Pipelines can also be added to ``s.once`` and ``s.last``, which are executed before and after the input sequences are processed, respectively:

.. code-block:: cpp

    s.once | ...  // executed once, at the start
    s.last | ...  // executed once, at the end

For example, one might use ``s.once`` to declare an array that is updated by each input sequence, then serialize this array in ``s.last`` (these topics are covered later).

-----

:doc:`next <stages>`
