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
#include "klee/Expr/ExprHashMap.h"
#include "klee/Internal/Support/IndependentElementSet.h"
#include <unordered_set>

namespace klee {

class ExprVisitor;
class ConstraintManager {
public:
  using iterator = Constraints_ty::iterator;
  using const_iterator = Constraints_ty::const_iterator;

  ConstraintManager() = default;
  ConstraintManager &operator=(const ConstraintManager &cs) = default;
  ConstraintManager(ConstraintManager &&cs) = default;
  ConstraintManager &operator=(ConstraintManager &&cs) = default;

  // create from constraints with no optimization
  explicit
  ConstraintManager(const Constraints_ty &_constraints);
  ConstraintManager(const ConstraintManager &cs);

  // Destructor
  ~ConstraintManager();

  typedef Constraints_ty::const_iterator constraint_iterator;
  typedef IndepElemSetPtrSet_ty::const_iterator factor_iterator;

  // given a constraint which is known to be valid, attempt to
  // simplify the existing constraint set
  void simplifyForValidConstraint(ref<Expr> e);

  ref<Expr> simplifyExpr(ref<Expr> e) const;

  // \param[out] if constraint added successfully (if it is valid)
  bool addConstraint(ref<Expr> e);

  bool empty() const noexcept { return constraints.empty(); }
  const_iterator begin() const { return constraints.cbegin(); }
  const_iterator end() const { return constraints.cend(); }
  std::size_t size() const noexcept { return constraints.size(); }
  factor_iterator factor_begin() const { return indep_indexer.factors.begin(); }
  factor_iterator factor_end() const { return indep_indexer.factors.end(); }
  size_t factor_size() const { return indep_indexer.factors.size(); }

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
  // compute a set of IndependentElementSet based on a given set of constraints
  // Assumption: all given constraints are already managed by this
  // ConstraintManager
  void
  getRelatedIndependentElementSets(const Constraints_ty &constraints,
                                   IndepElemSetPtrSet_ty &out_elemsets) const;
  void dumpEqualities(const char *filename=nullptr);

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
    IndepElemSetPtrSet_ty factors;
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
  // @param[in] visitor: a pre-built ExprReplaceVisitor, which can rewrite
  // any constraint expressions with the newly added constraint
  // @param[in] to_replace: non-null if IndependentSolver is enabled so that we
  // can only work on Independent sets intersecting with the expr to be
  // replaced.
  // @param[out] deleteConstraints: output all deleted/replaced constraint
  // expressions
  // @param[out] toAddConstraints: output all new replacement constraint
  // expressions
  bool rewriteConstraints(ExprReplaceVisitorBase &visitor,
                          const IndependentElementSet *to_replace,
                          std::vector<ref<Expr>> &deleteConstraints,
                          std::vector<ref<Expr>> &toAddConstraints);

  // @param[in] e: the new constraint expression waiting to be added
  // @param[out] toAddConstraints: will append new rewritten constraints caused
  // by the constraint `e`
  bool addConstraintInternal(ref<Expr> e,
                             std::vector<ref<Expr>> &toAddConstraints);

  // when `UseIndependentSolver` is enabled, these two function digest
  // constraints changes
  // We add one constraint at a time (the argument e). All deleted constraints
  // are stored in `deleteConstraints`.
  // Replacement will be later added one by one until there is no new
  // replacements.
  void updateIndependentSetAdd(const ref<Expr> &e);
  void
  updateIndependentSetDelete(const std::vector<ref<Expr>> &deleteConstraints);
  // when `UseIndependentSolver` is disabled, I still need to digest constraints
  // changes to the set representative
  void updateDeleteAdd(const ref<Expr> &e,
                       const std::vector<ref<Expr>> &deleteConstraints);
  // maintain the equalities used to simplify(replace) expression
  void updateEqualities(const ref<Expr> &e,
                        const std::vector<ref<Expr>> &deleteConstraints);
};

} // namespace klee

#endif /* KLEE_CONSTRAINTS_H */

