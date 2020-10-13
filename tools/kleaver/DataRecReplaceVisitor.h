#ifndef  KLEE_DATARECREPLACEVISITOR_H
#define KLEE_DATARECREPLACEVISITOR_H
#include "klee/Expr/Constraints.h"
#include "klee/Expr/ExprReplaceVisitor.h"
#include "klee/util/OracleEvaluator.h"
#include "klee/util/RefHashMap.h"
using namespace klee;

class DataRecReplaceVisitor : public ExprReplaceVisitorBase {
    private:
        std::unordered_set<std::string> dataRecInstSet;
        OracleEvaluator oracle_eval;
        RefHashSet<Expr> new_constraints;
    public:
      DataRecReplaceVisitor(UNMap_ty &_replacedUN, UNMap_ty &_visitedUN,
                            std::string &datarecCFG, std::string &OracleKTest);
      Action visitExprPost(const Expr &e);
      const RefHashSet<Expr> &getNewConstraints() const {
        return new_constraints;
      }
};
#endif // KLEE_DATARECREPLACEVISITOR_H
