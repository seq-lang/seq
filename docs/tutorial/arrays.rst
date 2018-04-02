Arrays
======

Arrays can be declared like this:

.. code-block:: c++

    seq::Seq s;

    Var m = s.once | Int[10];
    ...

``s.once`` is for pipelines only to be executed once, such as declaring global memory. As an example, we can construct a simple index of 8-mers for an input FASTA file:

.. code-block:: cpp

    #include "seq.h"

    using namespace seq;
    using namespace seq::types;
    using namespace seq::stageutil;

    // simple 2-bit encoding
    SEQ_FUNC seq_int_t my_hash_func(char *seq, seq_int_t len)
    {
        seq_int_t h = 0;

        for (seq_int_t i = 0; i < len; i++) {
            h <<= 2;
            switch (seq[i]) {
                case 'A':
                case 'a':
                    h += 0;
                    break;
                case 'C':
                case 'c':
                    h += 1;
                    break;
                case 'G':
                case 'g':
                    h += 2;
                    break;
                case 'T':
                case 't':
                    h += 3;
                    break;
                default:
                    break;
            }
        }
        return h;
    }

    static Hash& my_hash()
    {
        return hash("my_hash_func", my_hash_func);
    }

    int main()
    {
        seq::Seq s;

        const unsigned K = 8;
        const unsigned N = 1 << (2 * K);  // 4^K

        /* build the index */
        Var index = s.once | Int[N];
        Pipeline kmers = s | split(K,1);
        Var h = kmers | my_hash();
        kmers | count() | index[h];  // store the kmer offset in our index

        s.source("input.fasta");
        s.execute();
    }

As a second example, let’s store the integers from 0 to 9 in an array and print them (again, boilerplate omitted for brevity):

.. code-block:: c++

    seq::Seq s;

    const unsigned N = 10;
    Var m = s.once | Int[N];

    s.once | range(N) | m[_];  // store m[i] = i
    s.once | range(N) | m[_] | print();

    s.source("input.fastq");
    s.execute();

Recall that ``_`` always refers to the output of the previous stage. Notice that we need the ``s.once |`` at the start of each pipeline so that they are associated with ``s``. This is sometimes implicit, as with variables (e.g. ``myvar | ...``) and loads (e.g. ``array[i] | ...``), but with stages it must be explicit.

Finally, note that ``collect()`` can be used to collect all the outputs of a given pipeline into an array. For example, let's say we wanted to store all non-overlapping 32-mers in an array, then print each as well as its reverse complement:

.. code-block:: c++

    seq::Seq s;

    Var kmers = s | split(32,32) | collect();
    kmers | foreach() | (print(), copy() | revcomp() | print());

    s.source("input.fastq");
    s.execute();

Notice also the use of ``foreach()``, which enables iteration over arrays.

We can also partition arrays by some key function. For example, let's say we wanted to partition all 5-mers based on their first base:

.. code-block:: c++

    Func key(types::Seq, types::Seq);
    key | substr(1,1);

    Var kmers = s | split(5,1) | collect();
    kmers | chunk(key) | (len() | print(), foreach() | print());

(Functions are described in more detail later.) This will output something like this:

.. code-block:: text

    3
    TTTCA
    TTCAT
    TCATT
    1
    CATTC
    1
    ATTCC
    2
    TTCCA
    TCCAT
    2
    CCATT
    CATTC

-----

:doc:`next <functions>`
