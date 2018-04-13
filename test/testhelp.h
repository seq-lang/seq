#ifndef SEQ_TESTHELP_H
#define SEQ_TESTHELP_H

#include <string>
#include <fstream>
#include <gtest/gtest.h>
#include <seq/seq.h>

using namespace seq;
using namespace seq::types;
using namespace seq::stageutil;

#define DEFAULT_TEST_INPUT "test/data/single/seqs.txt"

#define TEST_INPUTS_SINGLE "test/data/single/seqs.txt",   \
                           "test/data/single/seqs.fastq", \
                           "test/data/single/seqs.fasta"

#define TEST_INPUTS_MULTI  "test/data/multiple/seqs.txt",   \
                           "test/data/multiple/seqs.fastq", \
                           "test/data/multiple/seqs.fasta"

#endif /* SEQ_TESTHELP_H */
