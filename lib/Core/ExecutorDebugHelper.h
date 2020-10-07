#ifndef KLEE_EXECUTOR_DEBUGHELPER_H
#define KLEE_EXECUTOR_DEBUGHELPER_H
#include "llvm/IR/Instructions.h"
#include "klee/ExecutionState.h"
#include "klee/Expr/ExprDebugHelper.h"
void debugDumpLLVMIR(llvm::Instruction *llvmir);
void debugDumpLLVMValue(llvm::Value *V);
extern klee::ref<klee::Expr> debugExpr;
void debugDumpConstraintsEval(
    klee::ExecutionState &state, klee::ConstraintManager &cm,
    const std::vector<klee::ref<klee::Expr>> &expr_vec, const char *filename);
void debugDumpConstraints(klee::ExecutionState &state,
                          klee::ConstraintManager &cm,
                          klee::ref<klee::Expr> expr, const char *filename);
extern llvm::raw_ostream &debugLLVMErrs;
void printDebugLibVersion(llvm::raw_ostream &os);
void debugAnalyzeIndirectMemoryAccess(klee::ExecutionState &, llvm::raw_ostream &);
#endif
