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
// For each successful rewriting, the old expression should be added to
// `deletedConstraints` and deleted from constraints.
// The new replacement expression should be added to `toAddConstraints`.
// Other internal data structures (e.g. indep_indexer) should left untouched.
bool ConstraintManager::rewriteConstraints(
    ExprReplaceVisitorBase &visitor, const IndependentElementSet *to_replace,
    std::vector<ref<Expr>> &deleteConstraints,
    std::vector<ref<Expr>> &toAddConstraints) {
  bool changed = false;
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
    for (const IndependentElementSet *elemset : indep_elemsets) {
      for (const ref<Expr> &e : elemset->exprs) {
        ref<Expr> new_e = visitor.replace(e);
        if (new_e != e) {
          deleteConstraints.push_back(e);
          toAddConstraints.push_back(new_e);
          constraints.erase(e);
          changed = true;
        }
      }
    }
  } else {
    for (Constraints_ty::iterator it = constraints.begin(),
                                  ie = constraints.end();
         it != ie; ++it) {
      const ref<Expr> &e = *it;
      ref<Expr> new_e = visitor.replace(e);

      if (new_e != e) {
        // TODO: maybe I can check if the rewritten expr has the same
        // IndependentSet as previous one.
        deleteConstraints.push_back(e);
        toAddConstraints.push_back(new_e);
        constraints.erase(it++);
        changed = true;
      } else {
        ++it;
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
  if (!replaceVisitor) {
    replaceVisitor = new klee::ExprReplaceVisitorMulti(replacedUN, visitedUN, equalities);
  }
  ref<Expr> res = replaceVisitor->replace(e);
  return res;
}

// Precondition: constraint expression e has NOT been added to internal data
// structure.
// Postcondition: constraint expression e has been added to internal data
// structure. Replaced constraints have been deleted from internal data
// structure and the replacements are put in `toAddConstraints` by
// `rewiteConstraints`
bool ConstraintManager::addConstraintInternal(
    ref<Expr> e, std::vector<ref<Expr>> &toAddConstraints) {
  if (representative.find(e) != representative.end()) {
    // found a duplicated constraint, nothing changed, directly return
    return false;
  }
  // rewrite any known equalities and split Ands into different conjuncts
  bool changed = false;

  std::vector<ref<Expr>> deleteConstraints;
  switch (e->getKind()) {
  case Expr::Constant:
    assert(cast<ConstantExpr>(e)->isTrue() && 
           "attempt to add invalid (false) constraint");
    break;

    // split to enable finer grained independence and other optimizations
  case Expr::And: {
    BinaryExpr *be = cast<BinaryExpr>(e);
    changed |= addConstraintInternal(be->left, toAddConstraints);
    changed |= addConstraintInternal(be->right, toAddConstraints);
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
            changed |= rewriteConstraints(visitor, &to_replace,
                                          deleteConstraints, toAddConstraints);
          } else {
            changed |= rewriteConstraints(visitor, nullptr, deleteConstraints,
                                          toAddConstraints);
          }
        }
      }
    }
    constraints.insert(e);
    updateEqualities(e, deleteConstraints);
    if (UseIndependentSolver) {
      // should first process deleted constraints then newly added
      updateIndependentSetDelete(deleteConstraints);
      updateIndependentSetAdd(e);
    } else {
      updateDeleteAdd(e, deleteConstraints);
    }
    break;
  }

  default:
    // deleteConstraints should be empty here.
    constraints.insert(e);
    updateEqualities(e, deleteConstraints);
    if (UseIndependentSolver) {
      // should first process deleted constraints then newly added
      updateIndependentSetDelete(deleteConstraints);
      updateIndependentSetAdd(e);
    } else {
      updateDeleteAdd(e, deleteConstraints);
    }
    break;
  }
  return changed;
}

void ConstraintManager::updateIndependentSetDelete(
    const std::vector<ref<Expr>> &deleteConstraints) {
  // First find if there are removed constraint. If is, update the correspoding set.
  if (deleteConstraints.empty()) {
    return;
  }

  // I believe delete only happens when a constraint is rewritten. But I think
  // it is unlikely to break a IndependentSet if you rewrite a constraint.
  // TODO: do we really want to traverse the entire IndependentSet upon delete/rewrite?
  // Get the set for all the IndependentSet that need to be update.
  // key: factors need to update (first delete then rebuild)
  // value: set of constraints (expressions) to delete in the corresponding factor
  std::unordered_map<IndependentElementSet*, ExprHashSet > updateList;
  for (auto it = deleteConstraints.begin(); it != deleteConstraints.end(); it++) {
    auto find_it = representative.find(*it);
    assert(find_it != representative.end());
    IndependentElementSet *indep = find_it->second;
    assert(indep);
    updateList[indep].insert(*it);
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
void ConstraintManager::updateIndependentSetAdd(const ref<Expr> &e) {
  IndependentElementSet *current = new IndependentElementSet(e);
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
    for (auto &it : current->exprs) {
      representative[it] = singleIntersect;
    }
    delete current;
  } else {
    // expensive case: need to merge multiple intersected independent sets
    for (IndependentElementSet *indepSet : indep_elemsets) {
      current->add(*indepSet);
    }
    // Update representative and factors.
    for (auto it = current->exprs.begin(); it != current->exprs.end(); it++) {
      representative[*it] = current;
    }

    for (IndependentElementSet *victim : indep_elemsets) {
      indep_indexer.erase(victim);
      delete victim;
    }

    indep_indexer.insert(current);
  }
}

void ConstraintManager::updateDeleteAdd(
    const ref<Expr> &e, const std::vector<ref<Expr>> &deleteConstraints) {
  for (const ref<Expr> &e : deleteConstraints) {
    assert(representative.erase(e) && "Seems like representative out of sync");
  }
  representative[e] = nullptr;
}

void ConstraintManager::updateEqualities(
    const ref<Expr> &e, const std::vector<ref<Expr>> &deleteConstraints) {
  { // add one new constraint e
    bool isConstantEq = false;
    if (const EqExpr *EE = dyn_cast<EqExpr>(e)) {
      if (isa<ConstantExpr>(EE->left)) {
        equalities[EE->right] = EE->left;
        isConstantEq = true;
      }
    }
    if (!isConstantEq) {
      equalities[e] = ConstantExpr::alloc(1, Expr::Bool);
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

bool ConstraintManager::addConstraint(ref<Expr> e) {
  CompareCacheSemaphoreHolder CCSH;
  if (representative.find(e) != representative.end()) {
    // found a duplicated constraint
    return true;
  }

  std::vector<ref<Expr>> toAddConstraints;
  toAddConstraints.push_back(e);
  volatile size_t cnt = 0;
  while (!toAddConstraints.empty()) {
    ++cnt;
    ref<Expr> toAdd_e = toAddConstraints.back();
    toAddConstraints.pop_back();
    ref<Expr> simplified = simplifyExpr(toAdd_e);
    if (simplified->isFalse()) {
      return false;
    } else if (simplified->isTrue()) {
      continue;
    }
    bool changed __attribute__((unused)) =
        addConstraintInternal(simplified, toAddConstraints);
  }
  if (UseIndependentSolver && EnableIndepExprCheckSum) {
    // Check factors are exclusive (sum the number of constraints and compare
    // with representative.size())
    size_t allNExprs = representative.size();
    assert(allNExprs == constraints.size());
    indep_indexer.checkExprsSum(allNExprs);
  }
  return true;
}

void ConstraintManager::getRelatedIndependentElementSets(
    const Constraints_ty &constraints,
    IndepElemSetPtrSet_ty &out_elemsets) const {
  for (const ref<Expr> &e : constraints) {
    auto find_it = representative.find(e);
    assert(find_it != representative.end() &&
           "Input constriants are not a subset of what are managed by this "
           "ConstraintManager");
    out_elemsets.insert(find_it->second);
  }
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
