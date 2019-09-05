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

k-mer extraction
----------------

.. code-block:: seq

    myseq  = s'CAATAGAGACTAAGCATTAT'
    type K = Kmer[5]
    stride = 2

    # explicit for-loop:
    for subseq in myseq.kmers[K](stride):
        print subseq

    # pipelined:
    myseq |> kmers[K](stride) |> echo

Reverse complementation
-----------------------

.. code-block:: seq

    # sequences:
    s = s'GGATC'
    print ~s     # GATCC
    s.revcomp()  # in-place rev. comp.
    print s      # GATCC

    # k-mers
    k = Kmer[5](s'GGATC')
    print ~k     # GATCC

k-mer Hamming distance
----------------------

.. code-block:: seq

    type K = Kmer[5]
    k1 = K(s'ACGTC')
    k2 = K(s'ACTTA')
    #          ^ ^
    print abs(k1 - k2)  # Hamming distance = 2

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

Reverse-complement palindrome test
----------------------------------

.. code-block:: seq

    def is_own_revcomp(s: seq) -> bool:
        match s:
            case s'A...T' or s'T...A' or s'C...G' or s'G...C':
                return is_own_revcomp(s[1:-1])
            case s'':
                return True
            default:
                return False

Reading FASTA/FASTQ
-------------------

.. code-block:: seq

    # iterate over sequences:
    for s in FASTA('genome.fa'):
        print s

    # iterate over everything:
    for r in FASTA('genome.fa').all():
        print r.name()
        print r.seq()

    # iterate over sequences:
    for s in FASTQ('reads.fq'):
        print s

    # iterate over everything:
    for r in FASTQ('reads.fq').all():
        print r.name()
        print r.read()
        print r.qual()

Reading paired-end FASTQ
------------------------

.. code-block:: seq

    # iterate over sequences:
    for s1, s2 in zip(FASTQ('reads_1.fq'), FASTQ('reads_2.fq')):
        print s1, s2

    # iterate over everything:
    for r1, r2 in zip(FASTQ('reads_1.fq').all(), FASTQ('reads_2.fq').all()):
        print r1.name(), r2.name()
        print r1.read(), r2.read()
        print r1.qual(), r2.qual()

Parallel FASTQ processing
-------------------------

.. code-block:: seq

    def process(s: seq):
        ...
    # OMP_NUM_THREADS environment variable controls threads
    fastq('reads.fq') ||> process

    # Sometimes batching reads into blocks can improve performance,
    # especially if each is quick to process.
    fastq('reads.fq') |> block(1000) ||> process

Reading SAM/BAM/CRAM
--------------------

.. code-block:: seq

    # iterate over sequences:
    for s in SAM('alignments.sam'):
        print s

    for s in BAM('alignments.bam'):
        print s

    for s in CRAM('alignments.cram'):
        print s

    # iterate over everything:
    for r in SAM('alignments.sam').all():
        print r.name()
        print r.read()
        print r.pos()
        print r.mapq()
        print r.cigar()
        print r.reversed()
        # etc.

    for r in BAM('alignments.bam').all():
        # ...

    for r in CRAM('alignments.cram').all():
        # ...
