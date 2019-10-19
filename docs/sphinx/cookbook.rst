Cookbook
========

.. contents::

Subsequence extraction
----------------------

.. code-block:: seq

    myseq  = s'CAATAGAGACTAAGCATTAT'
    sublen = 5
    stride = 2

    # explicit for-loop
    for subseq in myseq.split(sublen, stride):
        print subseq

    # pipelined
    myseq |> split(sublen, stride) |> echo

k-mer extraction
----------------

.. code-block:: seq

    myseq  = s'CAATAGAGACTAAGCATTAT'
    type K = Kmer[5]
    stride = 2

    # explicit for-loop
    for subseq in myseq.kmers[K](stride):
        print subseq

    # pipelined
    myseq |> kmers[K](stride) |> echo

Reverse complementation
-----------------------

.. code-block:: seq

    # sequences
    s = s'GGATC'
    print ~s     # GATCC

    # k-mers
    k = k'GGATC'
    print ~k     # GATCC

k-mer Hamming distance
----------------------

.. code-block:: seq

    k1 = k'ACGTC'
    k2 = k'ACTTA'
    #        ^ ^
    print abs(k1 - k2)  # Hamming distance = 2

k-mer Hamming neighbors
-----------------------

.. code-block:: seq

    def neighbors(kmer):
        for i in range(len(kmer)):
            for b in (k'A', k'C', k'G', k'T'):
                if kmer[i] != b:
                    yield kmer |> base(i, b)

    print list(neighbors(k'AGC'))  # CGC, GGC, etc.

k-mer minimizer
---------------

.. code-block:: seq

    def minimizer[K](s: seq):
        assert len(s) >= K.len()
        kmer_min = K(s)
        for kmer in s[1:].kmers[K](1):
            kmer = min(kmer, ~kmer)
            if kmer < kmer_min: kmer_min = kmer
        return kmer_min

    print minimizer[Kmer[10]](s)

Count bases
-----------

.. code-block:: seq

    type BaseCount(A: int, C: int, G: int, T: int):
        def __add__(self: BaseCount, other: BaseCount):
            a1, c1, g1, t1 = self
            a2, c2, g2, t2 = other
            return (a1 + a2, c1 + c2, g1 + g2, t1 + t2)

    def count_bases(s: seq) -> BaseCount:
        match s:
            case s'A...': return count_bases(s[1:]) + (1,0,0,0)
            case s'C...': return count_bases(s[1:]) + (0,1,0,0)
            case s'G...': return count_bases(s[1:]) + (0,0,1,0)
            case s'T...': return count_bases(s[1:]) + (0,0,0,1)
            default: return BaseCount(0,0,0,0)

Spaced seed search
------------------

.. code-block:: seq

    def has_spaced_acgt(s: seq) -> bool:
        match s:
            case s'A_C_G_T...':
                return True
            case t if len(t) >= 8:
                return has_spaced_acgt(s[1:])
            default:
                return False

Reverse-complement palindrome
-----------------------------

.. code-block:: seq

    def is_own_revcomp(s: seq) -> bool:
        match s:
            case s'A...T' or s'T...A' or s'C...G' or s'G...C':
                return is_own_revcomp(s[1:-1])
            case s'':
                return True
            default:
                return False

Sequence alignment
------------------

.. code-block:: seq

    # default parameters
    s1 = s'CGCGAGTCTT'
    s2 = s'CGCAGAGTT'
    aln = s1 @ s2
    print aln.cigar, aln.score

    # custom parameters
    # match = 2; mismatch = 4; gap1(k) = 4k + 2; gap2(k) = 13k + 1
    config = AlignConfig(2, 4).gap1(4, 2).gap2(13, 1)
    aln = s1.align_dual(s2, config)
    print aln.cigar, aln.score

Reading FASTA/FASTQ
-------------------

.. code-block:: seq

    # iterate over everything
    for r in FASTA('genome.fa'):
        print r.name
        print r.seq

    # iterate over sequences
    for s in FASTA('genome.fa') |> seqs:
        print s

    # iterate over everything
    for r in FASTQ('reads.fq'):
        print r.name
        print r.read
        print r.qual

    # iterate over sequences
    for s in FASTQ('reads.fq') |> seqs:
        print s

Reading paired-end FASTQ
------------------------

.. code-block:: seq

    for r1, r2 in zip(FASTQ('reads_1.fq'), FASTQ('reads_2.fq')):
        print r1.name, r2.name
        print r1.read, r2.read
        print r1.qual, r2.qual

Parallel FASTQ processing
-------------------------

.. code-block:: seq

    def process(s: seq):
        ...

    # OMP_NUM_THREADS environment variable controls threads
    FASTQ('reads.fq') |> iter ||> process

    # Sometimes batching reads into blocks can improve performance,
    # especially if each is quick to process.
    FASTQ('reads.fq') |> iter |> block(1000) ||> process

Reading SAM/BAM/CRAM
--------------------

.. code-block:: seq

    # iterate over everything
    for r in SAM('alignments.sam'):
        print r.name
        print r.read
        print r.pos
        print r.mapq
        print r.cigar
        print r.reversed
        # etc.

    for r in BAM('alignments.bam'):
        # ...

    for r in CRAM('alignments.cram'):
        # ...

    # iterate over sequences
    for s in SAM('alignments.sam') |> seqs:
        print s

    for s in BAM('alignments.bam') |> seqs:
        print s

    for s in CRAM('alignments.cram') |> seqs:
        print s
