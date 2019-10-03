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
    KTest *ktest;
    typedef std::unordered_map<std::string, unsigned int> arrayname2idx_ty;
    arrayname2idx_ty arrayname2idx;

    protected:
    ref<Expr> getInitialValue(const Array &mo, unsigned index);

    public:
    OracleEvaluator(std::string KTestPath, bool silent = false);
  };
}
#endif
