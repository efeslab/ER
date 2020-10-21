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
#include "klee/Internal/Module/KModule.h"
#include "klee/OptionCategories.h"
#include "klee/Solver/SolverCmdLine.h"

#include "llvm/IR/Function.h"
#include "llvm/Support/CommandLine.h"

#include <map>
#include <unordered_map>
#include <unordered_set>
#include "klee/Expr/ExprHashMap.h"
#include "klee/util/RefHashMap.h"

using namespace klee;

namespace {
llvm::cl::opt<bool> RewriteEqualities(
    "rewrite-equalities",
    llvm::cl::desc("Rewrite existing constraints when an equality with a "
                   "constant is added (default=true)"),
    llvm::cl::init(true),
    llvm::cl::cat(SolvingCat));
}

// Non-null `to_replace` implies `UseIndependentSolver`
bool ConstraintManager::rewriteConstraints(
    ExprReplaceVisitorBase &visitor, const IndependentElementSet *to_replace,
    IndepElemSetPtrSet_ty *intersected_factors) {
  bool changed = false;

  // we will update constraints by making a copy and regenerate its content
  Constraints_ty swap_temp;
  constraints.swap(swap_temp);
  if (to_replace) {
    assert(to_replace->exprs.size() == 1 &&
           "Should only replace one Expr at a time");
    assert(intersected_factors &&
           "Null intersected_factors while to_replace is non-null");
    for (IndependentElementSet *elemset : factors) {
      if (to_replace->intersects(*elemset)) {
        intersected_factors->insert(elemset);
      }
    }
    Constraints_ty intersected_temp;
    intersected_temp.swap(swap_temp);
    for (ref<Expr> &e : intersected_temp) {
      if (intersected_factors->count(representative[e])) {
        swap_temp.push_back(e);
      } else {
        // ignore unrelated exprs by removing them from the copy `swap_temp`
        // and putting them in constraints directly;
        constraints.push_back(e);
      }
    }
  }

  for (ConstraintManager::constraints_ty::iterator it = swap_temp.begin(),
                                                   ie = swap_temp.end();
       it != ie; ++it) {
    ref<Expr> &ce = *it;
    ref<Expr> e = visitor.replace(ce);

    if (e != ce) {
      // TODO: maybe I can check if the rewritten expr has the same
      // IndependentSet as previous one.
      addConstraintInternal(e); // enable further reductions
      changed = true;
    } else {
      constraints.push_back(ce);
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
  if (!replaceVisitor) {
    replaceVisitor = new klee::ExprReplaceVisitorMulti(replacedUN, visitedUN, equalities);
  }
  ref<Expr> res = replaceVisitor->replace(e);
  return res;
}

bool ConstraintManager::addConstraintInternal(ref<Expr> e) {
  if (representative.find(e) != representative.end()) {
    // found a duplicated constraint, nothing changed, directly return
    return false;
  }
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
      if (ConstantExpr *CE = dyn_cast<ConstantExpr>(be->left)) {
        if (!CE->isFalse()) {
          ExprReplaceVisitorSingle visitor(replacedUN, visitedUN, be->right,
                                     be->left);
          visitedUN.clear();
          if (replaceVisitor)
            replaceVisitor->resetVisited();
          if (UseIndependentSolver) {
            // create a new IndependentElementSet of the expr to be replaced
            IndependentElementSet to_replace(be->right);
            auto insert_it = intersected_factors_cache.insert(
                std::make_pair(e, IndepElemSetPtrSet_ty()));
            IndepElemSetPtrSet_ty &intersected_factors =
                insert_it.first->second;
            changed |=
                rewriteConstraints(visitor, &to_replace, &intersected_factors);
          } else {
            changed |= rewriteConstraints(visitor, nullptr, nullptr);
          }
        }
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

void ConstraintManager::updateIndependentSetDelete() {
  // First find if there are removed constraint. If is, update the correspoding set.
  if (deleteConstraints.empty()) {
    return;
  }

  // I believe delete only happens when a constraint is rewritten. But I think
  // it is unlikely to break a IndependentSet if you rewrite a constraint.
  // TODO: do we really want to traverse the entire IndependentSet upon delete/rewrite?
  // Get the set for all the IndependentSet that need to be update.
  // key: factors need to update
  // value: set of constraints (expressions) to delete in the corresponding factor
  std::unordered_map<IndependentElementSet*, ExprHashSet > updateList;
  for (auto it = deleteConstraints.begin(); it != deleteConstraints.end(); it++) {
    updateList[representative[*it]].insert(*it);
    // Update representative.
    assert(representative.erase(*it) &&
           "Seems like representative out of sync");
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
void ConstraintManager::updateIndependentSetAdd() {
  while (!addedConstraints.empty()) {
    ref<Expr> e = addedConstraints.back();
    addedConstraints.pop_back();
    IndependentElementSet* current = new IndependentElementSet(e);
    // garbage consists of existing factors which are intersected with
    // this new constraint
    // We will use cached results when available (change garbage_p)
    IndepElemSetPtrSet_ty garbage;
    IndepElemSetPtrSet_ty *garbage_p = &garbage;
    auto find_it = intersected_factors_cache.find(e);
    if (find_it != intersected_factors_cache.end()) {
      // cache hit, we have run intersects on this new constraint before
      garbage_p = &(find_it->second);
    } else {
      // cache miss, we need to calculate the intersected factors here
      for (auto it = factors.begin(); it != factors.end(); it++) {
        if (current->intersects(*(*it))) {
          garbage_p->insert(*it);
        }
      }
    }

    if (garbage_p->size() == 1) {
      // lucky and cheap case: newly added constraint falls exactly in one
      // existing factor we can reuse the existing factor
      IndependentElementSet *singleIntersect = *(garbage_p->begin());
      singleIntersect->add(*current);
      for (auto &it: current->exprs) {
        representative[it] = singleIntersect;
      }
      delete current;
    } else {
      // expensive case: need to merge multiple intersected independent sets
      for (IndependentElementSet *indepSet: *(garbage_p)) {
        current->add(*indepSet);
      }
      // Update representative and factors.
      for (auto it = current->exprs.begin(); it != current->exprs.end(); it ++ ) {
        representative[*it] = current;
      }

      for (IndependentElementSet *victim: *(garbage_p)) {
        factors.erase(victim);
        delete victim;
      }

      factors.insert(current);
    }
  }
}

void ConstraintManager::updateDeleteAdd() {
  for (ref<Expr> &e : deleteConstraints) {
    assert(representative.erase(e) && "Seems like representative out of sync");
  }
  for (ref<Expr> &e : addedConstraints) {
    representative[e] = nullptr;
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

void ConstraintManager::checkConstraintChange(const Constraints_ty &old) {
  ExprHashSet oldConsSet(old.begin(), old.end());

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

bool ConstraintManager::addConstraint(ref<Expr> e) {
  CompareCacheSemaphoreHolder CCSH;
  if (representative.find(e) != representative.end()) {
    // found a duplicated constraint
    return true;
  }
  // make a copy of current constraint set. Will be used to calculate
  // constraint changes if rewritting changes any constraint.
  Constraints_ty old(constraints);
  // After update the independant, the add and delete vector should be cleaned;
  assert(deleteConstraints.empty() && "delete Constraints not empty"); 
  assert(addedConstraints.empty() && "add Constraints not empty"); 

  ref<Expr> simplified = simplifyExpr(e);
  if (simplified->isFalse()) {
    return false;
  }
  bool changed = addConstraintInternal(simplified);

  // If the constraints are changed by rewriteConstraints. Check what has been
  // modified;
  if (changed) {
    addedConstraints.clear();
    checkConstraintChange(old);
  }

  updateEqualities();

  // NOTE that updateIndependentSet will destroy addedConstraints and
  // deletedConstraints. Thus I should updateEqualities before this.
  if (UseIndependentSolver) {
    // should first process deleted constraints then newly added
    updateIndependentSetDelete();
    updateIndependentSetAdd();
  } else {
    updateDeleteAdd();
  }

  addedConstraints.clear();
  deleteConstraints.clear();
  intersected_factors_cache.clear();
  // TODO Check factors are exclusive (sum the number of constraints and compare with representative.size())
  return true;
}

ConstraintManager::ConstraintManager(const std::vector< ref<Expr> > &_constraints) :
  constraints(_constraints) {
    // Need to establish factors and representative
    std::vector<IndependentElementSet*> temp;
    for (auto it = _constraints.begin(); it != _constraints.end(); it ++ ) {
      temp.push_back(new IndependentElementSet(*it));
    }

    std::vector<IndependentElementSet*> result;
    if (!temp.empty()) {
      result.push_back(temp.back());
      temp.pop_back();
    }
    // work like this:
    // assume IndependentSet was setup for each constraint independently,
    // represented by I0, I1, ... In
    // temp initially has: I1...In
    // result initially has: I0
    // then for each IndependentSet Ii in temp, scan the entire result vector,
    // combine any IndependentSet in result intersecting with Ii and put Ii
    // in the result vector.
    //
    // result vector should only contain exclusive IndependentSet all the time.

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

    for (auto r = result.begin(); r != result.end(); r++) {
      factors.insert(*r);
      for (auto e = (*r)->exprs.begin(); e != (*r)->exprs.end(); e++) {
        representative[*e] = *r;
      }
    }
  }

ConstraintManager::ConstraintManager(const ConstraintManager &cs) : constraints(cs.constraints) {
  // Copy constructor needs to make deep copy of factors and representative
  // Here we assume every IndependentElementSet point in representative also exist in factors.
  for (auto it = cs.factors.begin(); it != cs.factors.end(); it++) {
    IndependentElementSet* candidate = new IndependentElementSet(*(*it));
    factors.insert(candidate);
    for (auto e = candidate->exprs.begin(); e != candidate->exprs.end(); e++) {
      representative[*e] = candidate;
    }
  }
}

// Destructor
ConstraintManager::~ConstraintManager() {
  // Here we assume every IndependentElementSet point in representative also exist in factors.
  for (auto it = factors.begin(); it != factors.end(); it++) {
    delete(*it);
  }
  if (replaceVisitor) {
    delete replaceVisitor;
  }
  // TODO Double check your assumption
}
