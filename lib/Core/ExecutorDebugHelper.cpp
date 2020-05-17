#include "ExecutorDebugHelper.h"
#include <fstream>

#include "llvm/Support/raw_ostream.h"

#include "klee/Expr/ExprPPrinter.h"
#include "klee/util/ExprConcretizer.h"
#include "ExecutorCmdLine.h"
using namespace klee;

void debugDumpLLVMIR(llvm::Instruction *llvmir) {
    llvmir->print(llvm::errs());
    llvm::errs() << '\n';
}
void debugDumpLLVMValue(llvm::Value *V) {
  V->print(llvm::errs(), true);
}

/// Dump constraints to a file, used inside gdb
///
/// \param[in] state Contains all symbolic variables we are interested in.
/// \param[in] cm Contains all constraints, not necessary to be state.constraints
/// \param[in] expr_vec: if non-empty, it has the exprs you want to evaluate and 
///   the dumped query will only evaluate given exprs
///   if empty, the query will evaluate `false` and ask for assignment of every
///   symbolic value.
/// \param[in] filename The path and name of the dumped file. Default is "constraints.txt"
/// FIXME: this is no longer a debug helper. iterative constraints
///   simplification depends on this function to dump the constraints from on going
///   symbolic replay.
void debugDumpConstraintsEval(ExecutionState &state, ConstraintManager &cm, const std::vector<ref<Expr>> &expr_vec, const char *filename) {
  std::string str;
  llvm::raw_string_ostream os(str);
  std::ofstream ofs(filename);
  if (!ofs.good()) {
    llvm::errs() << "open file " << filename << "failed\n";
    return;
  }
  const ref<Expr> *evalExprsBegin = nullptr;
  const ref<Expr> *evalExprsEnd = nullptr;
  const Array *const *evalArraysBegin = nullptr;
  const Array *const *evalArraysEnd = nullptr;
  std::vector<const Array*> symbolic_objs;
  std::vector<ref<Expr>> simplified_expr_vec;
  for (const ref<Expr> &e: expr_vec) {
    simplified_expr_vec.push_back(cm.simplifyExpr(e));
  }
  if (simplified_expr_vec.empty()) { // no query expr, dumping for initial assigment
    for (auto s: state.symbolics) {
      symbolic_objs.push_back(s.second);
    }
    if (!symbolic_objs.empty()) {
      evalArraysBegin = &(symbolic_objs[0]);
      evalArraysEnd = evalArraysBegin + symbolic_objs.size();
    }
  }
  else { // has query expr, dumping for evaluation
    evalExprsBegin = &simplified_expr_vec[0];
    evalExprsEnd = evalExprsBegin + simplified_expr_vec.size();
  }
  ExprPPrinter::printQuery(os, cm.getAllConstraints(), ConstantExpr::alloc(false, Expr::Bool),
      evalExprsBegin, evalExprsEnd,
      evalArraysBegin, evalArraysEnd, true);
  ofs << os.str();
  ofs.close();
}
/// Similar to above, but easier to use in gdb
/// \param[in] expr If non-null, it's the expr you want to evaluate and
///   the dumped query will not ask for symbolic values' assignment.
///   If null, the query will evalute `false` and ask for assignment.
ref<Expr> debugExpr = ref<Expr>(0);
void debugDumpConstraints(ExecutionState &state, ConstraintManager &cm, ref<Expr> expr, const char *filename) {
  std::vector<ref<Expr>> exprs;
  if (!expr.isNull()) {
    exprs.push_back(expr);
  }
  debugDumpConstraintsEval(state, cm, exprs, filename);
}

llvm::raw_ostream &debugLLVMErrs = llvm::errs();

void printDebugLibVersion(llvm::raw_ostream &os) {
  os << "Supported Debug Func:\n"
     << "\tdebugDumpConstraints: " << reinterpret_cast<void*>(&debugDumpConstraints) << '\n'
     << "\tdebugDumpLLVMIR: " << reinterpret_cast<void*>(&debugDumpLLVMIR) << '\n'
     << "\tdebugDumpLLVMValue: " << reinterpret_cast<void*>(&debugDumpLLVMValue) << '\n';
}

void debugAnalyzeIndirectMemoryAccess(ExecutionState &state, llvm::raw_ostream &os) {
  ConstraintManager &cm = state.constraints;

  IndirectReadDepthCalculator c1(cm.getAllConstraints());
  auto lastLevelReads = c1.getLastLevelReads();
  for (auto it = lastLevelReads.begin(), ie = lastLevelReads.end();
            it != ie; it++) {
    const ref<Expr> &e = *it;
    e->print(os);
    os << " : " << c1.query(e) << "\n";
  }
  os << "max : " << c1.getMax() << "\n";

#if 0
  ExprConcretizer ace(OracleKTest);
  ace.addConcretizedInputValue("A-data", 13);
  auto newConstraints = ace.evaluate(constraints);

  IndirectReadDepthCalculator c2(newConstraints);
  lastLevelReads = c2.getLastLevelReads();
  for (auto it = lastLevelReads.begin(), ie = lastLevelReads.end();
            it != ie; it++) {
    const ref<Expr> &e = *it;
    e->print(os);
    os << " : " << c2.query(e) << "\n";
  }
  os << "max : " << c2.getMax() << "\n";
#endif
}
