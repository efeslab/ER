//===-- Constraints.cpp ---------------------------------------------------===//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "klee/Expr/Constraints.h"

#include "klee/Expr/ExprPPrinter.h"
#include "klee/Expr/ExprVisitor.h"
#include "klee/Internal/Module/KModule.h"
#include "klee/OptionCategories.h"
#include "klee/Solver/SolverCmdLine.h"

#include "llvm/IR/Function.h"
#include "llvm/Support/CommandLine.h"

#include <map>
#include <unordered_map>
#include <unordered_set>
#include "klee/Expr/ExprHashMap.h"

using namespace klee;

namespace {
llvm::cl::opt<bool> RewriteEqualities(
    "rewrite-equalities",
    llvm::cl::desc("Rewrite existing constraints when an equality with a "
                   "constant is added (default=true)"),
    llvm::cl::init(true),
    llvm::cl::cat(SolvingCat));
}

/**
 * ExprReplaceVisitor can find and replace occurrence of a single expression.
 */
class ExprReplaceVisitor : public ExprVisitor {
private:
  ref<Expr> src, dst;

public:
  ExprReplaceVisitor(ref<Expr> _src, ref<Expr> _dst) : src(_src), dst(_dst) {}

  Action visitExpr(const Expr &e) {
    if (e == *src.get()) {
      return Action::changeTo(dst);
    } else {
      return Action::doChildren();
    }
  }

  Action visitExprPost(const Expr &e) {
    if (e == *src.get()) {
      return Action::changeTo(dst);
    } else {
      return Action::doChildren();
    }
  }
};

/**
 * ExprReplaceVisitor2 can find and replace multiple expressions.
 * The replacement mapping is passed in as a std::map
 */
class ExprReplaceVisitor2 : public ExprVisitor {
private:
  const std::map< ref<Expr>, ref<Expr> > &replacements;

public:
  ExprReplaceVisitor2(const std::map< ref<Expr>, ref<Expr> > &_replacements) 
    : ExprVisitor(true),
      replacements(_replacements) {}

  Action visitExprPost(const Expr &e) {
    std::map< ref<Expr>, ref<Expr> >::const_iterator it =
      replacements.find(ref<Expr>(const_cast<Expr*>(&e)));
    if (it!=replacements.end()) {
      return Action::changeTo(it->second);
    } else {
      return Action::doChildren();
    }
  }
};

bool ConstraintManager::rewriteConstraints(ExprVisitor &visitor) {
  bool changed = false;
  if (old.empty()) {
    constraints.swap(old);
    for (ConstraintManager::constraints_ty::iterator 
           it = old.begin(), ie = old.end(); it != ie; ++it) {
      ref<Expr> &ce = *it;
      ref<Expr> e = visitor.visit(ce);

      if (e!=ce) {
        addConstraintInternal(e); // enable further reductions
        changed = true;
      } else {
        constraints.push_back(ce);
      }
    }
  } else {
    ConstraintManager::constraints_ty old_temp;
    constraints.swap(old_temp);
    for (ConstraintManager::constraints_ty::iterator 
           it = old_temp.begin(), ie = old_temp.end(); it != ie; ++it) {
      ref<Expr> &ce = *it;
      ref<Expr> e = visitor.visit(ce);

      if (e!=ce) {
        addConstraintInternal(e); // enable further reductions
        changed = true;
      } else {
        constraints.push_back(ce);
      }
    }
  }

  return changed;
}

void ConstraintManager::simplifyForValidConstraint(ref<Expr> e) {
  // XXX 
}

ref<Expr> ConstraintManager::simplifyExpr(ref<Expr> e) const {
  if (isa<ConstantExpr>(e))
    return e;
  return ExprReplaceVisitor2(equalities).visit(e);
}

bool ConstraintManager::addConstraintInternal(ref<Expr> e) {
  // rewrite any known equalities and split Ands into different conjuncts
  bool changed = false;
  switch (e->getKind()) {
  case Expr::Constant:
    assert(cast<ConstantExpr>(e)->isTrue() && 
           "attempt to add invalid (false) constraint");
    break;

    // split to enable finer grained independence and other optimizations
  case Expr::And: {
    BinaryExpr *be = cast<BinaryExpr>(e);
    changed |= addConstraintInternal(be->left);
    changed |= addConstraintInternal(be->right);
    break;
  }

  case Expr::Eq: {
    if (RewriteEqualities) {
      // XXX: should profile the effects of this and the overhead.
      // traversing the constraints looking for equalities is hardly the
      // slowest thing we do, but it is probably nicer to have a
      // ConstraintSet ADT which efficiently remembers obvious patterns
      // (byte-constant comparison).
      BinaryExpr *be = cast<BinaryExpr>(e);
      if (isa<ConstantExpr>(be->left) && !isa<EqExpr>(be->right)) {
        ExprReplaceVisitor visitor(be->right, be->left);
        changed |= rewriteConstraints(visitor);
      }
    }
    constraints.push_back(e);
    addedConstraints.push_back(e);
    break;
  }

  default:
    constraints.push_back(e);
    addedConstraints.push_back(e);
    break;
  }
  return changed;
}

void ConstraintManager::updateDelete() {
  // Get the set for all the IndependentSet that need to be update.
  std::unordered_map<IndependentElementSet*, ExprHashSet > updateList;
  for (auto it = deleteConstraints.begin(); it != deleteConstraints.end(); it++) {
    updateList[representative[*it]].insert(*it);
    // Update representative.
    representative.erase(*it);
  }

  // Update factors.
  for (auto it = updateList.begin(); it != updateList.end(); it++) {
    factors.erase(it->first);
  }

  for (auto it = updateList.begin(); it != updateList.end(); it++) {
    std::vector<IndependentElementSet*> temp;
    for (auto e = it->first->exprs.begin(); e != it->first->exprs.end(); e++) {
      auto find = it->second.find(*e);
      if (find == it->second.end()) {
        temp.push_back(new IndependentElementSet(*e));
      }
    }

    std::vector<IndependentElementSet*> result;

    if (!temp.empty()) {
      result.push_back(temp.back());
      temp.pop_back();
    }

    while (!temp.empty()) {
      IndependentElementSet* current = temp.back();
      temp.pop_back();
      unsigned int i = 0;
      while (i < result.size()) {
        if (current->intersects(*result[i])) {
          current->add(*result[i]);
          IndependentElementSet* victim = result[i];
          result[i] = result.back();
          result.pop_back();
          delete victim;
        } else {
          i++;
        }
      }
      result.push_back(current);
    }

    // Update representative and factors.
    for (auto r = result.begin(); r != result.end(); r++) {
      factors.insert(*r);
      for (auto e = (*r)->exprs.begin(); e != (*r)->exprs.end(); e++) {
          representative[*e] = *r;
      }
    }
  }
}

// Update the representative after adding constraint
void ConstraintManager::updateIndependentSet() {
  // First find if there are removed constraint. If is, update the correspoding set.
  if (!deleteConstraints.empty()) {
    updateDelete();
  }

  while (!addedConstraints.empty()) {
    IndependentElementSet* current = new IndependentElementSet(addedConstraints.back());
    addedConstraints.pop_back();
    std::vector<IndependentElementSet*> garbage;
    for (auto it = factors.begin(); it != factors.end(); it++ ) {
      if (current->intersects(*(*it))) {
        current->add(*(*it));
        garbage.push_back(*it);
      }
    }

    // Update representative and factors.
    for (auto it = current->exprs.begin(); it != current->exprs.end(); it ++ ) {
      representative[*it] = current;
    }

    while (!garbage.empty()) {
      IndependentElementSet* victim = garbage.back();
      garbage.pop_back();
      factors.erase(victim);
      delete victim;
    }

    factors.insert(current);
  }
}

void ConstraintManager::updateEqualities() {
  for (auto &refE: addedConstraints) {
    bool isConstantEq = false;
    if (const EqExpr *EE = dyn_cast<EqExpr>(refE)) {
      if (isa<ConstantExpr>(EE->left)) {
        equalities[EE->right] = EE->left;
        isConstantEq = true;
      }
    }
    if (!isConstantEq) {
      equalities[refE] = ConstantExpr::alloc(1, Expr::Bool);
    }
  }
  for (auto refE: deleteConstraints) {
    bool isConstantEq = false;
    if (const EqExpr *EE = dyn_cast<EqExpr>(refE)) {
      if (isa<ConstantExpr>(EE->left)) {
        equalities.erase(EE->right);
        isConstantEq = true;
      }
    }
    if (!isConstantEq) {
      equalities.erase(refE);
    }
  }
}

void ConstraintManager::checkConstraintChange() {
  ExprHashSet oldConsSet;
  while (!old.empty()) {
    oldConsSet.insert(old.back());
    old.pop_back();
  }

  for (auto it = constraints.begin(); it != constraints.end(); it++ ) {
    if (!oldConsSet.erase(*it)) {
      addedConstraints.push_back(*it);
    } 
  }

  // The rest element inside oldConsSet should be the deleted element.
  for (auto itr = oldConsSet.begin(); itr != oldConsSet.end(); ++itr) {
    deleteConstraints.push_back(*itr);
  }
}

void ConstraintManager::addConstraint(ref<Expr> e) {
  // After update the independant, the add and delete vector should be cleaned;
  assert(old.empty() && "old vector is not empty"); 
  assert(deleteConstraints.empty() && "delete Constraints not empty"); 
  assert(addedConstraints.empty() && "add Constraints not empty"); 

  e = simplifyExpr(e);
  bool changed = addConstraintInternal(e);

  // If the constraints are changed by rewriteConstraints. Check what has been
  // modified; Important clear the old vector after finish running.
  if (changed) {
    addedConstraints.clear();
    checkConstraintChange();
  }
  old.clear();

  if (UseIndependentSolver) {
    updateIndependentSet();
  }
  updateEqualities();

  addedConstraints.clear();
  deleteConstraints.clear();
}

