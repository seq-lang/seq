Workshop
========

In this workshop, we will build a toy read mapper called *SeqMap* to
showcase some of Seq's domain-specific features and optimizations.


Getting Started
---------------

Let's create a new directory to house our Seq code and test data:

.. code:: bash

    mkdir seqmap
    cd seqmap

Now create a file ``seqmap.seq`` in this directory and open it with your
favorite editor.

We'll test *SeqMap* on the following data:

- Chromosome 22 as the reference sequence (``chr22.fa``, 52MB)
- A simulated set of 1 million 101bp reads (``reads.fastq``, 251MB)

Let's download this data into a new folder called ``data``:

.. code:: bash

    wget -c http://seq.csail.mit.edu/data.tar.gz -O - | tar -xz

What does this data actually look like? Let's take a look:

.. code:: bash

    head data/reads.fq

.. code:: text

    @chr22_16993648_16994131_1:0:0_2:0:0_0/1
    CTACCAAACACCTACTTCGTTTCCTAACATCACTTTAATTTTATCTTAGAGGAATTCTTTTCCCTATCCCATTAAGTTATGGGAGATGGGGCCAGGCATGG
    +
    55555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555
    @chr22_28253010_28253558_1:0:0_0:0:0_1/1
    AGTGTTTTGCCTGTGGCTAGACTAAAAATAAGGAATGAGGGGGGTATCTTCCACTCTTGCCCTCTCATCACCCTATTCCCTATATCCAGAACTCAGAGTCC
    +
    55555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555
    @chr22_21656192_21656802_0:0:0_2:0:0_2/1
    ATAGCGTGGATTCCTATGACATCAAGGAGCTATTTTATTTGGTAAAACGAAAAAGCACAATAATGAACGAACGCAAGCACTGAAACAGTGGAGACACCTAG

Each `FASTQ <https://en.wikipedia.org/wiki/FASTQ_format>`_ record consists of four lines:

- Read name, starting with ``@``. Notice that since this data is simulated, the read name includes the
  location on the genome where the read comes from; this will be useful later!
- Read sequence. This is a DNA sequence consisting of ``ACGT`` bases and possibly ``N``, indicating an
  ambiguous base.
- Separator (``+``)
- Read quality scores. This is a string of characters as long as the read sequence, where each character
  indicates the "quality" of the corresponding base in the read sequence. We won't be using it here.

What about the reference sequence `FASTA <https://en.wikipedia.org/wiki/FASTA_format>`_ file?

.. code:: bash

    head data/chr22.fa

.. code:: text

    >chr22
    NNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNN
    NNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNN
    NNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNN
    NNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNN
    NNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNN
    NNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNN
    NNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNN
    NNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNN
    NNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNN

FASTQ records start with the sequence name (prefixed with ``>``) followed by the sequence itself, split
across multiple lines. But why are all the bases ``N``? Chromosomal sequences often have their starts
and ends masked with ``N``'s to cover repetitive `telomeric <https://en.wikipedia.org/wiki/Telomere>`_ sequences.
Since we usually don't want to include such regions in our analyses, they are masked in the file. Let's
look a bit further down:

.. code:: bash

    head -n 1000000 data/chr22.fa | tail -n 10

.. code:: text

    tattaaaggaaaaaactgtatgaaatagtacatttctcataattctcatt
    ttgtaaaaataaagtacttatctatggacataatgagaaaatgactcaag
    gtaccaagagtttagccattagctataccagtggattataagcaaattct
    gttACGTGCATGCACTCACCTACGCATGTTCATGTATTCATACATACGTA
    CATAATTTTTTAAATTTTCTTTTATAGACAAGCAATAGCTTTATAATCTC
    TATAATCAGTAAAAATAAGTAAGTggctggacgcagtggctcacacctgt
    aatctcagcactttgggaggctgaggagggcagattatgaggtcagaaga
    tcaagaccatcctggctaacacagtgaaaccccatctctactaaaaatac
    aaaaaattagccacgcgtggtggcacgcgcctgtagtcccagctactggg
    gaggctgaggcaggaaaatcgcttgaacccgggaggcagaggttgcggtg

Now we can see the usual ``ACGT`` bases. The fact that some bases are lowercase indicates that they
are a part of some repetitive element or region. Seq will handle these different uppercase and lowercase
characters automatically, so we don't need to worry about them.

You might notice an additional file called ``chr22.fa.fai``: this is a FASTA index file that includes
information about each sequence contained in the file for easier parsing. We won't use it directly,
but Seq uses it internally to make FASTA parsing more efficient.


Section 1: Reading sequences from disk
--------------------------------------

The first step of processing any kind of sequencing data is to read it from disk.
Seq has builtin support for many of the standard file formats such as FASTA, FASTQ,
SAM, BAM, etc.

Let's write a program to read our FASTQ file and print each record's name and sequence
on a single line:

.. code:: seq

    from sys import argv
    for record in FASTQ(argv[1]):
        print record.name, record.seq

Now we can run this Seq program:

.. code:: bash

    seqc seqmap.seq data/reads.fq > out.txt

and view the results:

.. code:: bash

    head out.txt

.. code:: text

    chr22_16993648_16994131_1:0:0_2:0:0_0/1 CTACCAAACACCTACTTCGTTTCCTAACATCACTTTAATTTTATCTTAGAGGAATTCTTTTCCCTATCCCATTAAGTTATGGGAGATGGGGCCAGGCATGG
    chr22_28253010_28253558_1:0:0_0:0:0_1/1 AGTGTTTTGCCTGTGGCTAGACTAAAAATAAGGAATGAGGGGGGTATCTTCCACTCTTGCCCTCTCATCACCCTATTCCCTATATCCAGAACTCAGAGTCC
    chr22_21656192_21656802_0:0:0_2:0:0_2/1 ATAGCGTGGATTCCTATGACATCAAGGAGCTATTTTATTTGGTAAAACGAAAAAGCACAATAATGAACGAACGCAAGCACTGAAACAGTGGAGACACCTAG
    chr22_44541236_44541725_0:1:0_0:0:0_3/1 CTCTCTGTCTCTCTCTCTCCCCTAGGTCAGGGTGGTCCCTGGGGAGGCCCCTGGGTTACCCCAAGACAGGTGGGAGGTGCTTCCTACCCGACCCTCTTCCT
    chr22_39607671_39608139_0:0:0_2:0:0_4/1 ATTGGCTCAGAGTTCAGCAGGCTGTACCAGCATGGCGCCAGTGTCTGCTCCTGGTGAGGCCTTACGGACGTTACAATAACGGCGGAAGGCAAAGGCGGAGC
    chr22_35577703_35578255_3:0:0_1:0:0_5/1 TGCCATGGTGGTTAGCTGCACCCATCAACCTGTCATCTACATTAGGTATTTTTCCTAATGCTATCCCTCCCCTAGCACCCTACCCTCTGATAGGCCCTGGT
    chr22_46059124_46059578_1:0:0_1:0:0_6/1 AATCAGTACCAAACAATATATGGATATTATTGGCACTTTGTGCTCCCTCTGCCTGAACTGGGAATTCCTCTATTAGTTTTGACATTATCTGGTATTGAACC
    chr22_31651867_31652385_2:0:0_2:0:0_7/1 ATCTAGTGACAGTAAGTGGCTGATAAAGTGAGCTGCCATTACATAGTCATCATCTTTAATAGAAGTTAACACATACTGAGTTTCTACTATATTGGGTCTTT
    chr22_24816466_24817026_1:0:0_1:0:0_8/1 CACCTCTAGGGCTCAAGGGGCAGTTCCTCCATTCCTCAGCAGTGGCGCCTGTGGAACTGTGTCCTGAGGCCAGGGGGTGGTCAGGCAGGGCCTGGAGTGGC
    chr22_27496272_27496752_1:0:0_1:0:0_9/1 CTTAGCCCCATTAAACACTGGCAGGGCTGAATTGTCTGCTGCCATCCATCACACCTTCTCCCCTAGCCTGGTTTCTTACCTACCTGGAAGCCGTCCCTTTT

Pretty straightforward! FASTA files can be read in a very similar way.

Full code listing
~~~~~~~~~~~~~~~~~

.. code:: seq

    # SeqMap
    # Seq tutorial -- Section 1
    # Reads and prints a FASTQ file.
    # Usage: seqc seqmap.seq <FASTQ path>
    from sys import argv
    for record in FASTQ(argv[1]):
        print record.name, record.seq


Section 2: Building an index
----------------------------

Our goal is to find a "mapping" on the genome for each read. Comparing to every
position on the reference sequence would take far too long. An alternative is
to create an index of the k-mers from the reference sequence and use it to guide
the mapping process.

Let's build a dictionary that maps each k-mer to its position ("locus") on the
reference sequence:

.. code:: seq

    from sys import argv
    type K = Kmer[32]
    index = dict[K,int]()

    for record in FASTA(argv[1]):
        for pos,kmer in record.seq.kmers_with_pos[K](step=1):
            index[kmer] = pos

Of course, there will be k-mers that appear multiple times, but let's ignore this
detail for now and just store the latest position we see for each k-mer.

Another important issue is *reverse complementation*: some of our reads will map
in the reverse direction rather than in the forward direction. For this reason,
let's build our index in such a way that a k-mer is considered "equal" to its
reverse complement. One easy way to do this is by using "canonical" k-mers, i.e.
the minimum of a k-mer and its reverse complement:

.. code:: seq

    from sys import argv
    type K = Kmer[32]
    index = dict[K,int]()

    for record in FASTA(argv[1]):
        for pos,kmer in record.seq.kmers_with_pos[K](step=1):
            index[min(kmer, ~kmer)] = pos  # <--

(We'll have to use canonical k-mers when querying the index too, of course.)

Now we have our index as a dictionary (``index``), but we don't want to build it
each time we perform read mapping, since it only depends on the (fixed) reference
sequence. So, as a last step, let's dump the index to a file using the ``pickle``
module:

.. code:: seq

    import pickle
    import gzip

    with gzip.open(argv[1] + '.index', 'wb') as jar:
        pickle.dump(index, jar)

Run the program:

.. code:: bash

    seqc seqmap.seq data/chr22.fa

Now we should see a new file ``data/chr22.fa.index`` which stores our
serialized index.

The nice thing is we should only have to build our index once!

Full code listing
~~~~~~~~~~~~~~~~~

.. code:: seq

    # SeqMap
    # Seq tutorial -- Section 2
    # Reads and constructs a hash table index from an input
    # FASTA file.
    # Usage: seqc seqmap.seq <FASTA path> <FASTQ path>
    from sys import argv
    import pickle
    import gzip

    type K = Kmer[32]
    index = dict[K,int]()

    for record in FASTA(argv[1]):
        for pos,kmer in record.seq.kmers_with_pos[K](step=1):
            index[min(kmer, ~kmer)] = pos

    with gzip.open(argv[1] + '.index', 'wb') as jar:
        pickle.dump(index, jar)


Section 3: Finding k-mer matches
--------------------------------

At this point, we have an index we can load from disk. Let's use it
to find candidate mappings for our reads.

We'll split each read into k-mers and report a mapping if at least two
k-mers support a particular locus.

The first step is to load the index:

.. code:: seq

    from sys import argv
    import pickle
    import gzip

    type K = Kmer[32]
    index: dict[K,int] = None

    with gzip.open(argv[1] + '.index', 'rb') as jar:
        index = pickle.load[dict[K,int]](jar)

Now we can iterate over our reads and query k-mers in the index. We need
a way to keep track of candidate mapping positions as we process the
k-mers of a read: we can do this using a new dictionary, ``candidates``,
which maps candidate alignment positions to the number of k-mers supporting
the given position.

Then, we just iterate over ``candidates`` and output positions supported by
2 or more k-mers. Finally, we clear ``candidates`` before processing the next
read:

.. code:: seq

    candidates = dict[int,int]()  # position -> count mapping
    for record in FASTQ(argv[2]):
        for pos,kmer in record.read.kmers_with_pos[K](step=1):
            found = index.get(min(kmer, ~kmer), -1)
            if found > 0:
                candidates.increment(found - pos)

        for pos,count in candidates.items():
            if count > 1:
                print record.name, pos + 1

        candidates.clear()

Run the program:

.. code:: bash

    seqc seqmap.seq data/chr22.fa data/reads.fq > out.txt

Let's take a look at the output:

.. code:: bash

    head out.txt

.. code:: text

    chr22_16993648_16994131_1:0:0_2:0:0_0/1 16993648
    chr22_28253010_28253558_1:0:0_0:0:0_1/1 28253010
    chr22_44541236_44541725_0:1:0_0:0:0_3/1 44541236
    chr22_31651867_31652385_2:0:0_2:0:0_7/1 31651867
    chr22_21584577_21585142_1:0:0_1:0:0_a/1 21584577
    chr22_46629499_46629977_0:0:0_2:0:0_b/1 47088563
    chr22_46629499_46629977_0:0:0_2:0:0_b/1 51103174
    chr22_46629499_46629977_0:0:0_2:0:0_b/1 46795988
    chr22_16269615_16270134_0:0:0_1:0:0_c/1 50577316
    chr22_16269615_16270134_0:0:0_1:0:0_c/1 16269615

Notice that most positions we reported match the position from the read
name (the first integer after the ``_``); not bad!

Full code listing
~~~~~~~~~~~~~~~~~

.. code:: seq

    # SeqMap
    # Seq tutorial -- Section 3
    # Reads index constructed in Section 2 and looks up k-mers from
    # input reads to find candidate mappings.
    # Usage: seqc seqmap.seq <FASTA path> <FASTQ path>
    from sys import argv
    import pickle
    import gzip

    type K = Kmer[32]
    index: dict[K,int] = None

    with gzip.open(argv[1] + '.index', 'rb') as jar:
        index = pickle.load[dict[K,int]](jar)

    candidates = dict[int,int]()  # position -> count mapping
    for record in FASTQ(argv[2]):
        for pos,kmer in record.read.kmers_with_pos[K](step=1):
            found = index.get(min(kmer, ~kmer), -1)
            if found > 0:
                candidates.increment(found - pos)

        for pos,count in candidates.items():
            if count > 1:
                print record.name, pos + 1

        candidates.clear()


Section 4: Smith-Waterman alignment and CIGAR strings
-----------------------------------------------------

We now have the ability to report mapping *positions* for each read,
but usually we want *alignments*, which include information about
mismatches, insertions and deletions.

Luckily, Seq makes sequence alignment easy: to align sequence ``q``
against sequence ``t``, you can just do:

.. code:: seq

    aln = q @ t

``aln`` is a tuple of alignment score and CIGAR string (a *CIGAR string* is
a way of encoding an alignment result, and consists of operations such as ``M``
for match/mismatch, ``I`` for insertion and ``D`` for deletion, accompanied
by the number of associated bases; for example, ``3M2I4M`` indicates 3 (mis)matches
followed by a length-2 insertion followed by 4 (mis)matches).

By default, `Levenshtein distance <https://en.wikipedia.org/wiki/Levenshtein_distance>`_ is
used, meaning mismatch and gap costs are both 1, while match costs are zero. More
control over alignment parameters can be achieved using the ``align`` method:

.. code:: seq

    aln = q.align(t, a=2, b=4, ambig=0, gapo=4, gape=2)

where ``a`` is the match score, ``b`` is the mismatch cost, ``ambig`` is the
ambiguous base (``N``) match score, ``gapo`` is the gap open cost and ``gape``
the gap extension cost (i.e. a gap of length ``k`` costs ``gapo + (k * gape)``).
There are many more parameters as well, controlling factors like alignment bandwidth,
Z-drop, global/extension alignment and more; check the standard library reference
for further details.

For now, we'll use a simple ``query.align(target)``:

.. code:: seq

    candidates = dict[int,int]()
    for record in FASTQ(argv[2]):
        for pos,kmer in record.read.kmers_with_pos[K](step=1):
            found = index.get(min(kmer, ~kmer), -1)
            if found > 0:
                candidates.increment(found - pos)

        for pos,count in candidates.items():
            if count > 1:
                # get query, target and align:
                query = record.read
                target = reference[pos:pos + len(query)]
                alignment = query.align(target)
                print record.name, pos + 1, alignment.score, alignment.cigar

        candidates.clear()

Run the program:

.. code:: bash

    seqc seqmap.seq data/chr22.fa data/reads.fq > out.txt

And let's take a look at the output once again:

.. code:: bash

    head out.txt

.. code:: text

    chr22_16993648_16994131_1:0:0_2:0:0_0/1 16993648 -1 101M
    chr22_28253010_28253558_1:0:0_0:0:0_1/1 28253010 -1 101M
    chr22_44541236_44541725_0:1:0_0:0:0_3/1 44541236 -1 101M
    chr22_31651867_31652385_2:0:0_2:0:0_7/1 31651867 -2 101M
    chr22_21584577_21585142_1:0:0_1:0:0_a/1 21584577 -1 101M
    chr22_46629499_46629977_0:0:0_2:0:0_b/1 47088563 -15 20M1I4M1D76M
    chr22_46629499_46629977_0:0:0_2:0:0_b/1 51103174 -11 20M1I4M1D76M
    chr22_46629499_46629977_0:0:0_2:0:0_b/1 46795988 -12 20M1I4M1D76M
    chr22_16269615_16270134_0:0:0_1:0:0_c/1 50577316 -14 101M
    chr22_16269615_16270134_0:0:0_1:0:0_c/1 16269615 0 101M

Most of the alignments contain only matches or mismatches (``M``), which
is to be expected as insertions and deletions are far less common. In fact,
the three mappings containing indels appear to be incorrect!

A more thorough mapping scheme would also look at alignment scores before
reporting mappings, although for the purposes of this workshop we'll ignore
such improvements.

Full code listing
~~~~~~~~~~~~~~~~~

.. code:: seq

    # SeqMap
    # Seq tutorial -- Section 4
    # Reads index constructed in Section 2 and looks up k-mers from
    # input reads to find candidate mappings, then performs alignment.
    # Usage: seqc seqmap.seq <FASTA path> <FASTQ path>
    from sys import argv
    import pickle
    import gzip

    type K = Kmer[32]
    index: dict[K,int] = None

    reference = s''
    for record in FASTA(argv[1]):
        reference = record.seq

    with gzip.open(argv[1] + '.index', 'rb') as jar:
        index = pickle.load[dict[K,int]](jar)

    candidates = dict[int,int]()
    for record in FASTQ(argv[2]):
        for pos,kmer in record.read.kmers_with_pos[K](step=1):
            found = index.get(min(kmer, ~kmer), -1)
            if found > 0:
                candidates.increment(found - pos)

        for pos,count in candidates.items():
            if count > 1:
                query = record.read
                target = reference[pos:pos + len(query)]
                alignment = query.align(target)
                print record.name, pos + 1, alignment.score, alignment.cigar

        candidates.clear()


Section 5: Using pipelines and parallelism
------------------------------------------

Pipelines are a very convenient Seq construct for expressing a variety
of algorithms and applications. In fact, *SeqMap* can be thought of as
a pipeline with the following stages:

- read a record from the FASTQ file,
- find candidate alignments by querying the index,
- perform alignment for mappings supported by 2+ k-mers and output results.

We can write this as a pipeline in Seq as follows:

.. code:: seq

    def find_candidates(record):
        candidates = dict[int,int]()
        for pos,kmer in record.read.kmers_with_pos[K](step=1):
            found = index.get(min(kmer, ~kmer), -1)
            if found > 0:
                candidates.increment(found - pos)
        return record, candidates

    def report_mappings(t):
        record, candidates = t
        for pos,count in candidates.items():
            if count > 1:
                query = record.read
                target = reference[pos:pos + len(query)]
                alignment = query.align(target)
                print record.name, pos + 1, alignment.score, alignment.cigar

    FASTQ(argv[2]) |> iter |> find_candidates |> report_mappings

Parallelism can be achieved using the parallel pipe operator, ``||>``, which
tells the compiler that all subsequent stages can be executed in parallel:

.. code:: seq

    FASTQ(argv[2]) |> iter ||> find_candidates |> report_mappings

Since the full program also involves loading the index, let's time the main
pipeline using the ``timing`` module:

.. code:: seq

    import timing
    with timing('mapping'):
        FASTQ(argv[2]) |> iter ||> find_candidates |> report_mappings

We can try this for different numbers of threads:

.. code:: bash

    export OMP_NUM_THREADS=1
    seqc seqmap.seq data/chr22.fa data/reads.fq > out.txt
    # mapping took 45.5079s

    export OMP_NUM_THREADS=2
    seqc seqmap.seq data/chr22.fa data/reads.fq > out.txt
    # mapping took 35.886s

Full code listing
~~~~~~~~~~~~~~~~~

.. code:: seq

    # SeqMap
    # Seq tutorial -- Section 5
    # Reads index constructed in Section 2 and looks up k-mers from
    # input reads to find candidate mappings, then performs alignment.
    # Implemented with Seq parallel pipelines.
    # Usage: seqc seqmap.seq <FASTA path> <FASTQ path>
    from sys import argv
    from time import timing
    import pickle
    import gzip

    type K = Kmer[32]
    index: dict[K,int] = None

    reference = s''
    for record in FASTA(argv[1]):
        reference = record.seq

    with gzip.open(argv[1] + '.index', 'rb') as jar:
        index = pickle.load[dict[K,int]](jar)

    def find_candidates(record):
        candidates = dict[int,int]()
        for pos,kmer in record.read.kmers_with_pos[K](step=1):
            found = index.get(min(kmer, ~kmer), -1)
            if found > 0:
                candidates.increment(found - pos)
        return record, candidates

    def report_mappings(t):
        record, candidates = t
        for pos,count in candidates.items():
            if count > 1:
                query = record.read
                target = reference[pos:pos + len(query)]
                alignment = query.align(target)
                print record.name, pos + 1, alignment.score, alignment.cigar

    with timing('mapping'):
        FASTQ(argv[2]) |> iter |> find_candidates |> report_mappings


Section 6: Domain-specific optimizations
----------------------------------------

Seq already performs numerous domain-specific optimizations under the hood.
However, we can give the compiler a hint in this case to perform one more:
*inter-sequence alignment*. This optimization entails batching sequences
prior to alignment, then aligning multiple pairs using a very fast SIMD
optimized alignment kernel.

In Seq, we just need one additional function annotation to tell the compiler
to perform this optimization:

.. code:: seq

    @inter_align
    def report_mappings(t):
        ...

Let's run the program with and without this optimization:

.. code:: seq

    # without @inter_align
    seqc seqmap.seq data/chr22.fa data/reads.fq > out.txt
    # mapping took 43.4457s

    # with @inter_align
    seqc seqmap.seq data/chr22.fa data/reads.fq > out.txt
    # mapping took 32.3241s

(The timings with inter-sequence alignment will depend on the SIMD instruction
sets your CPU supports; these numbers are from using AVX2.)

Full code listing
~~~~~~~~~~~~~~~~~

.. code:: seq

    # SeqMap
    # Seq tutorial -- Section 6
    # Reads index constructed in Section 2 and looks up k-mers from
    # input reads to find candidate mappings, then performs alignment.
    # Implemented with Seq parallel pipelines using inter-seq. alignment.
    # Usage: seqc seqmap.seq <FASTA path> <FASTQ path>
    from sys import argv
    from time import timing
    import pickle
    import gzip

    type K = Kmer[32]
    index: dict[K,int] = None

    reference = s''
    for record in FASTA(argv[1]):
        reference = record.seq

    with gzip.open(argv[1] + '.index', 'rb') as jar:
        index = pickle.load[dict[K,int]](jar)

    def find_candidates(record):
        candidates = dict[int,int]()
        for pos,kmer in record.read.kmers_with_pos[K](step=1):
            found = index.get(min(kmer, ~kmer), -1)
            if found > 0:
                candidates.increment(found - pos)
        return record, candidates

    @inter_align
    def report_mappings(t):
        record, candidates = t
        for pos,count in candidates.items():
            if count > 1:
                query = record.read
                target = reference[pos:pos + len(query)]
                alignment = query.align(target)
                print record.name, pos + 1, alignment.score, alignment.cigar

    def process_block(block):
        block |> find_candidates |> report_mappings

    with timing('mapping'):
        FASTQ(argv[2]) |> iter |> find_candidates |> report_mappings