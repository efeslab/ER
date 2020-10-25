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
    ExprReplaceVisitorBase &visitor, const IndependentElementSet *to_replace) {
  bool changed = false;

  // we will update constraints by making a copy and regenerate its content
  Constraints_ty swap_temp;
  constraints.swap(swap_temp);
  if (to_replace) {
    assert(to_replace->exprs.size() == 1 &&
           "Should only replace one Expr at a time");
    // calculate independent elements related to the expr to be relaced
    IndepElemSetPtrSet_ty indep_elemsets;
    indep_indexer.getIntersection(to_replace, indep_elemsets);
    if (DebugIndependentIntersection) {
      IndepElemSetPtrSet_ty intersection_slowcheck;
      for (auto it = factor_begin(); it != factor_end(); ++it) {
        IndependentElementSet *elemset = *it;
        if (to_replace->intersects(*elemset)) {
          intersection_slowcheck.insert(elemset);
        }
      }
      assert(indep_elemsets == intersection_slowcheck && "Indexer bug");
    }
    // filter constraints in related independent elements
    Constraints_ty intersected_temp;
    intersected_temp.swap(swap_temp);
    for (ref<Expr> &e : intersected_temp) {
      IndependentElementSet *indep_elemset = representative[e];
      if (indep_elemsets.count(indep_elemset)) {
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
            changed |=
                rewriteConstraints(visitor, &to_replace);
          } else {
            changed |= rewriteConstraints(visitor, nullptr);
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
    indep_indexer.erase(it->first);
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
      indep_indexer.insert(*r);
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
    // indep_elemsets consists of existing factors which are intersected with
    // this new constraint
    IndepElemSetPtrSet_ty indep_elemsets;
    indep_indexer.getIntersection(current, indep_elemsets);
    if (DebugIndependentIntersection) {
      IndepElemSetPtrSet_ty intersection_slowcheck;
      // cache miss, we need to calculate the intersected factors here
      for (auto it = factor_begin(); it != factor_end(); it++) {
        if (current->intersects(*(*it))) {
          intersection_slowcheck.insert(*it);
        }
      }
      assert(indep_elemsets == intersection_slowcheck && "Indexer BUG");
    }

    if (indep_elemsets.size() == 1) {
      // lucky and cheap case: newly added constraint falls exactly in one
      // existing factor we can reuse the existing factor
      IndependentElementSet *singleIntersect = *(indep_elemsets.begin());
      singleIntersect->add(*current);
      indep_indexer.redirect(current, singleIntersect);
      for (auto &it: current->exprs) {
        representative[it] = singleIntersect;
      }
      delete current;
    } else {
      // expensive case: need to merge multiple intersected independent sets
      for (IndependentElementSet *indepSet: indep_elemsets) {
        current->add(*indepSet);
      }
      // Update representative and factors.
      for (auto it = current->exprs.begin(); it != current->exprs.end(); it ++ ) {
        representative[*it] = current;
      }

      for (IndependentElementSet *victim: indep_elemsets) {
        indep_indexer.erase(victim);
        delete victim;
      }

      indep_indexer.insert(current);
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

  // Check factors are exclusive (sum the number of constraints and compare with
  // representative.size())
  size_t allNExprs = representative.size();
  assert(allNExprs == constraints.size());
  indep_indexer.checkExprsSum(allNExprs);
  return true;
}

ConstraintManager::ConstraintManager(const Constraints_ty &_constraints)
    : constraints(_constraints) {
  std::vector<IndependentElementSet *> init_indep;
  for (const ref<Expr> &e : _constraints) {
    init_indep.push_back(new IndependentElementSet(e));
  }
  for (IndependentElementSet *indep : init_indep) {
    IndepElemSetPtrSet_ty intersected;
    indep_indexer.getIntersection(indep, intersected);
    if (intersected.empty()) {
      // no intersection, just add
      indep_indexer.insert(indep);
    } else if (intersected.size() == 1) {
      // intersected with one element, easy merge
      IndependentElementSet *singleIntersect = *(intersected.begin());
      singleIntersect->add(*indep);
      indep_indexer.redirect(indep, singleIntersect);
      delete indep;
    } else {
      // intersected with multiple elements, complex merge
      for (IndependentElementSet *intersect : intersected) {
        indep->add(*intersect);
        indep_indexer.erase(intersect);
        delete intersect;
      }
      indep_indexer.insert(indep);
    }
  }
  for (auto it = factor_begin(); it != factor_end(); ++it) {
    for (const ref<Expr> &e : (*it)->exprs) {
      representative[e] = *it;
    }
  }
}

ConstraintManager::ConstraintManager(const ConstraintManager &cs) : constraints(cs.constraints) {
  // Copy constructor needs to make deep copy of factors and representative
  // Here we assume every IndependentElementSet point in representative also exist in factors.
  for (auto it = cs.factor_begin(); it != cs.factor_end(); it++) {
    IndependentElementSet* candidate = new IndependentElementSet(*(*it));
    indep_indexer.insert(candidate);
    for (auto e = candidate->exprs.begin(); e != candidate->exprs.end(); e++) {
      representative[*e] = candidate;
    }
  }
}

// Destructor
ConstraintManager::~ConstraintManager() {
  // Here we assume every IndependentElementSet point in representative also exist in factors.
  for (auto it = factor_begin(); it != factor_end(); it++) {
    delete(*it);
  }
  if (replaceVisitor) {
    delete replaceVisitor;
  }
  // TODO Double check your assumption
}

void ConstraintManager::IndepElementSetIndexer::insert(
    IndependentElementSet *indep) {
  redirect(indep, indep);
  factors.insert(indep);
}

void ConstraintManager::IndepElementSetIndexer::erase(IndependentElementSet *indep) {
  for (const Array *arr : indep->wholeObjects) {
    assert(elements_index.count(arr) == 0 &&
           "Removing invalid indep elemset: sparse elements is conflict with "
           "existing whole object");
    wholeObj_index.erase(arr);
  }
  for (auto &elem : indep->elements) {
    const Array *arr = elem.first;
    const DenseSet<unsigned> &index_set = elem.second;
    assert(wholeObj_index.count(arr) == 0 &&
           "Removing invalid indep elemset: whole object is conflict with "
           "existing sparse elements.");
    auto it = elements_index.find(arr);
    if (it != elements_index.end()) {
      for (unsigned index : index_set) {
        it->second[index] = nullptr;
      }
    }
  }
  factors.erase(indep);
}

void ConstraintManager::IndepElementSetIndexer::getIntersection(const IndependentElementSet *indep, IndepElemSetPtrSet_ty &out_intersected) const {
  // check wholeObject intersection
  for (const Array *arr : indep->wholeObjects) {
    auto wholeObj_it = wholeObj_index.find(arr);
    auto elem_it = elements_index.find(arr);
    if (wholeObj_it != wholeObj_index.end()) {
      // wholeObject intersects with wholeObject
      assert(elem_it == elements_index.end());
      out_intersected.insert(wholeObj_it->second);
    } else if (elem_it != elements_index.end()) {
      // wholeObject intersects with elements
      for (IndependentElementSet *p : elem_it->second) {
        if (p) {
          out_intersected.insert(p);
        }
      }
    }
  }
  for (auto &item : indep->elements) {
    const Array *arr = item.first;
    const DenseSet<unsigned> &index_set = item.second;
    auto wholeObj_it = wholeObj_index.find(arr);
    auto elem_it = elements_index.find(arr);
    if (wholeObj_it != wholeObj_index.end()) {
      // elements intersects with wholeObject
      assert(elem_it == elements_index.end());
      out_intersected.insert(wholeObj_it->second);
    } else if (elem_it != elements_index.end()) {
      // elements intersects with elements
      for (unsigned index : index_set) {
        IndependentElementSet *p = elem_it->second[index];
        if (p) {
          out_intersected.insert(p);
        }
      }
    }
  }
}

void ConstraintManager::IndepElementSetIndexer::redirect(const IndependentElementSet *src, IndependentElementSet *dst) {
  for (const Array *arr : src->wholeObjects) {
    assert(
        elements_index.count(arr) == 0 &&
        "Updating an invalid index: wholeObj and elements have the same Array");
    wholeObj_index[arr] = dst;
  }
  for (auto elem : src->elements) {
    const Array *arr = elem.first;
    DenseSet<unsigned> &index_set = elem.second;
    assert(
        wholeObj_index.count(arr) == 0 &&
        "Updating an invalid index: elements and wholeObj have the same Array");
    auto it = elements_index.emplace(std::make_pair(
        arr,
        std::move(std::vector<IndependentElementSet *>(arr->size, nullptr))));
    for (unsigned index : index_set) {
      it.first->second[index] = dst;
    }
  }
}

void ConstraintManager::IndepElementSetIndexer::checkExprsSum(size_t NExprs) {
  size_t n = 0;
  for (const IndependentElementSet *e : factors) {
    n += e->exprs.size();
  }
  assert(n == NExprs);
}

/*
 * Template instantiation for embedding useful debugging helpers
 * Avoid "cannot evaluate" problems in gdb
 */
// for representative
template class std::unordered_map<ref<klee::Expr>, klee::IndependentElementSet *, klee::util::RefHash<klee::Expr>, klee::util::RefCmp<klee::Expr> >;
// for factors
template class std::unordered_set<IndependentElementSet*>;
// for elements_index
template class std::unordered_map<const Array *, std::vector<IndependentElementSet*>>;
// for wholeObj_index
template class std::unordered_map<const Array *, IndependentElementSet*>;
