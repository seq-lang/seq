#pragma once

#include "util/common.h"

#if SEQ_HAS_TAPIR

/*
 * Adapted from Tapir OpenMP backend source
 */
#include "llvm/Support/CommandLine.h"
#include "llvm/Transforms/Tapir/OpenMPABI.h"

extern llvm::StructType *IdentTy;
extern llvm::FunctionType *Kmpc_MicroTy;
extern llvm::Constant *DefaultOpenMPPSource;
extern llvm::Constant *DefaultOpenMPLocation;
extern llvm::PointerType *KmpRoutineEntryPtrTy;

extern llvm::Type *getOrCreateIdentTy(llvm::Module *module);
extern llvm::Value *getOrCreateDefaultLocation(llvm::Module *module);
extern llvm::PointerType *getIdentTyPointerTy();
extern llvm::FunctionType *getOrCreateKmpc_MicroTy(llvm::LLVMContext &context);
extern llvm::PointerType *getKmpc_MicroPointerTy(llvm::LLVMContext &context);

extern llvm::cl::opt<bool> fastOpenMP;

namespace seq {
namespace tapir {
void resetOMPABI();
} // namespace tapir
} // namespace seq

#endif // SEQ_HAS_TAPIR
