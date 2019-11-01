#ifndef EXPR_ORACLEEVALUATOR_H
#define EXPR_ORACLEEVALUATOR_H
#include "klee/Internal/ADT/KTest.h"
#include "klee/util/ExprEvaluator.h"

#include <string>
#include <unordered_map>

using namespace klee;

namespace klee {
  extern llvm::cl::opt<std::string> OracleKTest;

  class OracleEvaluator : public ExprEvaluator {
    // KTest loaded from the given file
    KTest *ktest;

    protected:
    ref<Expr> getInitialValue(const Array &mo, unsigned index);
    // arrayname2idx maps symbolic objs' name in the ktest to their index in the ktest.
    typedef std::unordered_map<std::string, unsigned int> arrayname2idx_ty;
    arrayname2idx_ty arrayname2idx;

    Action visitPointer(const PointerExpr& pe) {
      return Action::changeTo(pe.toConstantExpr());
    }

    public:
    OracleEvaluator(std::string KTestPath, bool silent = false);
  };
}
#endif
