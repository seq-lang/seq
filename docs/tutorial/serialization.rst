Serialization
=============

Objects can be serialized, and later deserialized either in the same program or in a different one. For example, let's serialize and deserialize our 8-mer index from before:

.. code-block:: c++

    seq::Seq s;

    const unsigned K = 8;
    const unsigned N = 1 << (2 * K);  // 4^K

    /* build the index */
    Var index = s.once | Int[N];
    Pipeline kmers = s | split(K,1);
    Var h = kmers | my_hash();
    kmers | count() | index[h];  // store the kmer offset in our index

    /* store & read the index */
    s.last | index | ser("index.dat");
    Var newIndex = s.last | deser(Array.of(Int), "index.dat");

    s.source("input.fasta");
    s.execute();

``s.last`` is for pipelines to be executed once, after all input sequences are processed.
