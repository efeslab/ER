#include "klee/Expr/ExprDebugHelper.h"
#include "klee/Expr/ExprPPrinter.h"

#include "llvm/Support/raw_ostream.h"

#include <string>
#include <vector>
#include <fstream>
using namespace klee;

void debugDumpConstraintsImpl(const Constraints_ty &constraints,
                              const std::vector<const Array*> &symbolic_objs,
                              const char *filename) {
  std::vector<ref<Expr>> empty_exprs;
  debugDumpConstraintsImpl(constraints, symbolic_objs, empty_exprs, filename);
}
void debugDumpConstraintsImpl(const Constraints_ty &constraints,
                              const std::vector<const Array*> &symbolic_objs,
                              const std::vector<ref<Expr>> &query_exprs,
                              const char *filename) {
  const ref<Expr> *evalExprsBegin = nullptr;
  const ref<Expr> *evalExprsEnd = nullptr;
  const Array *const *evalArraysBegin = nullptr;
  const Array *const *evalArraysEnd = nullptr;
  if (query_exprs.empty()) { // no query expr, dumping for initial assigment
    if (!symbolic_objs.empty()) {
      evalArraysBegin = &(symbolic_objs[0]);
      evalArraysEnd = evalArraysBegin + symbolic_objs.size();
    }
  } else { // has query expr, dumping for evaluation
    evalExprsBegin = &(query_exprs[0]);
    evalExprsEnd = evalExprsBegin + query_exprs.size();
  }

  std::string str;
  llvm::raw_string_ostream os(str);
  std::ofstream ofs(filename);
  if (!ofs.good()) {
    llvm::errs() << "open file " << filename << "failed\n";
    return;
  }

  ExprPPrinter::printQuery(
      os, constraints, ConstantExpr::alloc(false, Expr::Bool), evalExprsBegin,
      evalExprsEnd, evalArraysBegin, evalArraysEnd, true);
  ofs << os.str();
  ofs.close();
}
