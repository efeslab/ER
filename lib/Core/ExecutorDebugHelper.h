#ifndef KLEE_DEBUGHELPER_H
#define KLEE_DEBUGHELPER_H
#include "llvm/IR/Instructions.h"
#include "klee/ExecutionState.h"
#include "klee/Constraints.h"
#include "klee/Expr.h"
using namespace llvm;
using namespace klee;
void debugDumpLLVMIR(llvm::Instruction *llvmir);
void debugDumpLLVMValue(llvm::Value *V);
extern ref<Expr> debugExpr;
void debugDumpConstraints(ExecutionState &state, ConstraintManager &cm, ref<Expr> expr, const char *filename);
extern raw_ostream &debugLLVMErrs;
void printDebugLibVersion(llvm::raw_ostream &os);
void debugAnalyzeIndirectMemoryAccess(ExecutionState &, llvm::raw_ostream &);
#endif
