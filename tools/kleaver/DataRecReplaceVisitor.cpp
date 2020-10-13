#include "DataRecReplaceVisitor.h"
#include <fstream>

DataRecReplaceVisitor::DataRecReplaceVisitor(UNMap_ty &_replacedUN,
                                             UNMap_ty &_visitedUN,
                                             std::string &datarecCFG,
                                             std::string &OracleKTest)
    : ExprReplaceVisitorBase(_replacedUN, _visitedUN),
      oracle_eval(OracleKTest) {
  assert(datarecCFG != "" && "Invalid DataRec config");
  std::ifstream f(datarecCFG);
  while (f.good()) {
    std::string linestr;
    std::getline(f, linestr);

    if (linestr.empty() || linestr[0] == '#') {
      continue;
    }

    dataRecInstSet.insert(linestr);
  }
  f.close();
}

ExprVisitor::Action DataRecReplaceVisitor::visitExprPost(const Expr &e) {
    ref<Expr> e_ref = ref<Expr>(const_cast<Expr*>(&e));
    if (dataRecInstSet.count(e.getKInstUniqueID())) {
        // match kinst in datarec.cfg, should be substituted
        ref<Expr> eval_val = oracle_eval.visit(e_ref);
        if (isa<ConstantExpr>(eval_val)) {
          new_constraints.insert(EqExpr::create(eval_val, e_ref));
          return Action::changeTo(eval_val);
        } else {
          llvm::errs()
              << "Warn: oracle eval returns non constant values for:\n";
          e.dump();
          return Action::doChildren();
        }
    } else {
        return Action::doChildren();
    }
}
