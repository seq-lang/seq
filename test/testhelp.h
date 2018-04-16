#ifndef SEQ_TESTHELP_H
#define SEQ_TESTHELP_H

#include <string>
#include <fstream>
#include <gtest/gtest.h>
#include <seq/seq.h>

using namespace seq;
using namespace seq::types;
using namespace seq::stageutil;

#define DEFAULT_TEST_INPUT_SINGLE TEST_DIR "/data/single/seqs.txt"

#define DEFAULT_TEST_INPUT_MULTI  TEST_DIR "/data/multiple/seqs.fastq"

#define TEST_INPUTS_SINGLE TEST_DIR "/data/single/seqs.txt",   \
                           TEST_DIR "/data/single/seqs.fastq", \
                           TEST_DIR "/data/single/seqs.fasta"

#define TEST_INPUTS_MULTI  TEST_DIR "/data/multiple/seqs.txt",   \
                           TEST_DIR "/data/multiple/seqs.fastq", \
                           TEST_DIR "/data/multiple/seqs.fasta"

#endif /* SEQ_TESTHELP_H */
