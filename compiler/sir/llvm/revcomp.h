#pragma once

#include "llvm.h"

namespace seq {
namespace ir {

/// Builds a table mapping 8-bit encoded 4-mers to reverse complement encoded 4-mers
/// @param module LLVM module to put the table in
/// @param name the variable name of the table
/// @return LLVM global variable representing the table
llvm::GlobalVariable *getRevCompTable(llvm::Module *module,
                                      const std::string &name = "seq.revcomp_table");

/// Generates code for k-mer reverse complement using bit shifts
/// @param k the k-mer length
/// @param self the k-mer value as a (2k)-bit integer
/// @param block LLVM block to generate the code in
/// @return the reverse complemented k-mer value as a (2k)-bit integer
llvm::Value *codegenRevCompByBitShift(const unsigned k, llvm::Value *self,
                                      llvm::BasicBlock *block);

/// Generates code for k-mer reverse complement using a lookup table
/// @param k the k-mer length
/// @param self the k-mer value as a (2k)-bit integer
/// @param block LLVM block to generate the code in
/// @return the reverse complemented k-mer value as a (2k)-bit integer
llvm::Value *codegenRevCompByLookup(const unsigned k, llvm::Value *self,
                                    llvm::BasicBlock *block);

/// Generates code for k-mer reverse complement using SIMD instructions
/// @param k the k-mer length
/// @param self the k-mer value as a (2k)-bit integer
/// @param block LLVM block to generate the code in
/// @return the reverse complemented k-mer value as a (2k)-bit integer
llvm::Value *codegenRevCompBySIMD(const unsigned k, llvm::Value *self,
                                  llvm::BasicBlock *block);

/// Generates code for k-mer reverse complement by selecting the best
/// method based on the k-mer length via experimentally-found heuristics
/// @param k the k-mer length
/// @param self the k-mer value as a (2k)-bit integer
/// @param block LLVM block to generate the code in
/// @return the reverse complemented k-mer value as a (2k)-bit integer
llvm::Value *codegenRevCompHeuristic(const unsigned k, llvm::Value *self,
                                     llvm::BasicBlock *block);
} // namespace ir
} // namespace seq
