//===-- Constraints.h -------------------------------------------*- C++ -*-===//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef KLEE_CONSTRAINTS_H
#define KLEE_CONSTRAINTS_H

#include "klee/Expr/Expr.h"
#include "klee/Expr/ExprReplaceVisitor.h"
#include "klee/Internal/Support/IndependentElementSet.h"
#include <unordered_set>

namespace klee {

class ExprVisitor;
// Constraints_ty is an abstract type representing a collection of constraints.
// (ref<Expr> or Expr or etc.)
// Currently it is a vector and other data structures are used to guarantee
// uniqueness.
// TODO: In the future I hope it can be changed to std::unordered_set
typedef std::vector<ref<Expr>> Constraints_ty;
class ConstraintManager {
public:
  using constraints_ty = std::vector<ref<Expr>>;
  using iterator = constraints_ty::iterator;
  using const_iterator = constraints_ty::const_iterator;

  ConstraintManager() = default;
  //ConstraintManager &operator=(const ConstraintManager &cs) = default;
  //ConstraintManager(ConstraintManager &&cs) = default;
  //ConstraintManager &operator=(ConstraintManager &&cs) = default;

  // create from constraints with no optimization
  explicit
  ConstraintManager(const std::vector< ref<Expr> > &_constraints);
  ConstraintManager(const ConstraintManager &cs);

  // Destructor
  ~ConstraintManager();

  typedef Constraints_ty::const_iterator constraint_iterator;
  typedef std::unordered_set<klee::IndependentElementSet*>::const_iterator factor_iterator;

  // given a constraint which is known to be valid, attempt to
  // simplify the existing constraint set
  void simplifyForValidConstraint(ref<Expr> e);

  ref<Expr> simplifyExpr(ref<Expr> e) const;

  // \param[out] if constraint added successfully (if it is valid)
  bool addConstraint(ref<Expr> e);

  bool empty() const noexcept { return constraints.empty(); }
  ref<Expr> back() const { return constraints.back(); }
  const_iterator begin() const { return constraints.cbegin(); }
  const_iterator end() const { return constraints.cend(); }
  std::size_t size() const noexcept { return constraints.size(); }
  factor_iterator factor_begin() const { return indep_indexer.factors.begin(); }
  factor_iterator factor_end() const { return indep_indexer.factors.end(); }

  bool operator==(const ConstraintManager &other) const {
    return constraints == other.constraints;
  }

  bool operator!=(const ConstraintManager &other) const {
    return constraints != other.constraints;
  }

  const Constraints_ty& getAllConstraints() const { return constraints; }
  // expose getIntersection to public, should only call it when
  // `UseIndependentSolver` is enabled
  void getIntersection(const IndependentElementSet *indep,
                       IndepElemSetPtrSet_ty &out_intersected) const {
    indep_indexer.getIntersection(indep, out_intersected);
  }

private:
  Constraints_ty constraints;
  // When `UseIndependentSolver` is disabled, representative serves as a set of
  // constraints for deduplication
  // When `UseIndependentSolver` is enabled, representative also track the
  // mapping from a constraint to its IndependentElementSet
  ExprHashMap<klee::IndependentElementSet*> representative;

  class IndepElementSetIndexer {
    friend class ConstraintManager;
    // A faster index of all IndependentElementSet::elements
    std::unordered_map<const Array *,
                       std::vector<klee::IndependentElementSet *>>
        elements_index;
    // A faster index of IndependentElementSet::wholeObjects
    std::unordered_map<const Array *, klee::IndependentElementSet *>
        wholeObj_index;
    std::unordered_set<klee::IndependentElementSet*> factors;
    public:
    void insert(IndependentElementSet *indep);
    void erase(IndependentElementSet *indep);
    // given a independent set `indep`, output all intersected elements
    void getIntersection(
        const IndependentElementSet *indep,
        IndepElemSetPtrSet_ty &out_intersected) const;
    // Insert `src` to our index, but instead of tracking `src`, we track `dst`.
    // Useful when merging two IndependentElementSet
    void redirect(const IndependentElementSet *src, IndependentElementSet *dst);
    void checkExprsSum(size_t NExprs);
  };

  IndepElementSetIndexer indep_indexer;

  std::vector<ref<Expr>> deleteConstraints;
  std::vector<ref<Expr>> addedConstraints;
  // equalities consists of EqExpr in current constraints.
  // For each item <key,value> in this map, ExprReplaceVisitorMulti can find
  // occurrences of "key" in an expression and replace it with "value"
  ExprHashMap<ref<Expr>> equalities;
  // std::map<ref<Expr>, ref<Expr>> equalities;
  // If we RewriteEqualities, replacedUN maps update lists content to a unique
  // allocate. This is mainly for optimization results deduplication. I do not
  // want UpdateList diverge in multiple paths when I am following a single path
  // in replay.
  mutable UNMap_ty replacedUN;
  mutable UNMap_ty visitedUN;
  mutable ExprReplaceVisitorMulti *replaceVisitor = nullptr;

  // returns true iff the constraints were modified
  // This function is only called when you want to rewrite existing constraints
  // based on a newly learnt equivalency
  // @param to_replace: non-null if IndependentSolver is enabled so that we
  // can only work on Independent sets intersecting with the expr to be
  // replaced.
  bool rewriteConstraints(ExprReplaceVisitorBase &visitor,
                          const IndependentElementSet *to_replace);

  bool addConstraintInternal(ref<Expr> e);

  // when `UseIndependentSolver` is enabled, these two function digest
  // constraints changes
  void updateIndependentSetAdd();
  void updateIndependentSetDelete();
  // when `UseIndependentSolver` is disabled, I still need to digest constraints
  // changes to the set representative
  void updateDeleteAdd();
  // maintain the equalities used to simplify(replace) expression
  void updateEqualities();

  void checkConstraintChange(const Constraints_ty &old);

};

} // namespace klee

#endif /* KLEE_CONSTRAINTS_H */

