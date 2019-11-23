//===-- IndependentSolver.cpp ---------------------------------------------===//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "independent-solver"
#include "klee/Solver.h"

#include "klee/Expr.h"
#include "klee/Constraints.h"
#include "klee/SolverImpl.h"
#include "klee/Internal/Support/Debug.h"
#include "klee/SolverStats.h"
#include "klee/TimerStatIncrementer.h"

#include "klee/util/ExprUtil.h"
#include "klee/util/Assignment.h"

#include "llvm/Support/raw_ostream.h"
#include <map>
#include <vector>
#include <ostream>
#include <list>

using namespace klee;
using namespace llvm;

// Breaks down a constraint into all of it's individual pieces, returning a
// list of IndependentElementSets or the independent factors.
//
// Caller takes ownership of returned std::list.
// static std::list<IndependentElementSet>*
// getAllIndependentConstraintsSets(const Query &query) {
//   std::list<IndependentElementSet> *factors = new std::list<IndependentElementSet>();
//   ConstantExpr *CE = dyn_cast<ConstantExpr>(query.expr);
//   if (CE) {
//     assert(CE && CE->isFalse() && "the expr should always be false and "
//                                   "therefore not included in factors");
//   } else {
//     ref<Expr> neg = Expr::createIsZero(query.expr);
//     factors->push_back(IndependentElementSet(neg));
//   }
// 
//   for (ConstraintManager::const_iterator it = query.constraints.begin(),
//                                          ie = query.constraints.end();
//        it != ie; ++it) {
//     // iterate through all the previously separated constraints.  Until we
//     // actually return, factors is treated as a queue of expressions to be
//     // evaluated.  If the queue property isn't maintained, then the exprs
//     // could be returned in an order different from how they came it, negatively
//     // affecting later stages.
//     factors->push_back(IndependentElementSet(*it));
//   }
// 
//   bool doneLoop = false;
//   do {
//     doneLoop = true;
//     std::list<IndependentElementSet> *done =
//         new std::list<IndependentElementSet>;
//     while (factors->size() > 0) {
//       IndependentElementSet current = factors->front();
//       factors->pop_front();
//       // This list represents the set of factors that are separate from current.
//       // Those that are not inserted into this list (queue) intersect with
//       // current.
//       std::list<IndependentElementSet> *keep =
//           new std::list<IndependentElementSet>;
//       while (factors->size() > 0) {
//         IndependentElementSet compare = factors->front();
//         factors->pop_front();
//         if (current.intersects(compare)) {
//           if (current.add(compare)) {
//             // Means that we have added (z=y)added to (x=y)
//             // Now need to see if there are any (z=?)'s
//             doneLoop = false;
//           }
//         } else {
//           keep->push_back(compare);
//         }
//       }
//       done->push_back(current);
//       delete factors;
//       factors = keep;
//     }
//     delete factors;
//     factors = done;
//   } while (!doneLoop);
// 
//   return factors;
// }
static std::list<IndependentElementSet>*
getAllIndependentConstraintsSets(const Query &query) {
  std::list<IndependentElementSet> *result= new std::list<IndependentElementSet>();
  ConstantExpr *CE = dyn_cast<ConstantExpr>(query.expr);
  IndependentElementSet current;
  if (CE) {
    assert(CE && CE->isFalse() && "the expr should always be false and "
                                  "therefore not included in factors");
  } else {
    ref<Expr> neg = Expr::createIsZero(query.expr);
    current = IndependentElementSet(neg);
  }

  for (ConstraintManager::factor_iterator it = query.constraints.factor_begin(), 
                ie = query.constraints.factor_end(); it != ie; ++it) {
    if (current.intersects(*(*it))) {
      current.add(*(*it));
    } else {
      result->push_back(*(*it));
    }
  }
  
  return result;
}

static 
IndependentElementSet getIndependentConstraints(const Query& query,
                                                std::vector< ref<Expr> > &result) {
  
  // IndependentElementSet eltsClosure(query.expr);
  // //std::vector< ref<Expr> > result;
  // std::vector< std::pair<ref<Expr>, IndependentElementSet> > worklist;
  // 
  // for (ConstraintManager::const_iterator it = query.constraints.begin(), 
  //        ie = query.constraints.end(); it != ie; ++it)
  //   worklist.push_back(std::make_pair(*it, IndependentElementSet(*it)));
  // 
  // // XXX This should be more efficient (in terms of low level copy stuff).
  // bool done = false;
  // do {
  //   done = true;
  //   std::vector< std::pair<ref<Expr>, IndependentElementSet> > newWorklist;
  //   for (std::vector< std::pair<ref<Expr>, IndependentElementSet> >::iterator
  //          it = worklist.begin(), ie = worklist.end(); it != ie; ++it) {
  //     if (it->second.intersects(eltsClosure)) {
  //       if (eltsClosure.add(it->second))
  //         done = false;
  //       result.push_back(it->first);
  //       // Means that we have added (z=y)added to (x=y)
  //       // Now need to see if there are any (z=?)'s
  //     } else {
  //       newWorklist.push_back(*it);
  //     }
  //   }
  //   worklist.swap(newWorklist);
  // } while (!done);
  
  // **********************************************************
  
  IndependentElementSet eltsClosure(query.expr);
  
  for (ConstraintManager::factor_iterator it = query.constraints.factor_begin(), 
                ie = query.constraints.factor_end(); it != ie; ++it) {
    if (eltsClosure.intersects(*(*it))) {
      //if (eltsClosure.add(*(*it))) {
        for (auto eb = (*it)->exprs.begin(), ee =  (*it)->exprs.end(); eb != ee; eb++) {
           result.push_back(*eb);
        //}
      }
    }
  }
  
  // // **********************************************************
  // // Compare independent result
  // std::set< ref<Expr> > reqset(result.begin(), result.end());
  // std::set< ref<Expr> > reqsetCompare(resultCompare.begin(), resultCompare.end());
  // for (std::set< ref<Expr> >::iterator it = reqset.begin(); it != reqset.end(); it++) {
  //     std::set< ref<Expr> >::iterator find = reqsetCompare.find(*it);
  //     // errs() << "1" << "\n";
  //     if (find == reqsetCompare.end()) {
  //         errs() << " " << (reqset.count(*it) ? "(required)" : "(independent)") << "\n";
  //         errs() << "\telts: " << IndependentElementSet(*it) << "\n";
  //         errs() << "--\n";
  //         errs() << "Q: " << query.expr << "\n";
  //         errs() << "\telts: " << IndependentElementSet(query.expr) << "\n";
  //         int i = 0;
  //         for (ConstraintManager::const_iterator itt = query.constraints.begin(),
  //             ie = query.constraints.end(); itt != ie; ++itt) {
  //           errs() << "C" << i++ << ": " << *itt;
  //           errs() << " " << (reqset.count(*itt) ? "(required)" : "(independent)") << "\n";
  //           errs() << "\telts: " << IndependentElementSet(*itt) << "\n";
  //         }
  //         errs() << "elts closure: " << eltsClosure << "\n";
  //         assert(0);
  //     } else {
  //         reqsetCompare.erase(*it);
  //     }
  // }
  // 
  // if (!reqsetCompare.empty()) {
  //     for (ConstraintManager::const_iterator itt = query.constraints.begin(),
  //         ie = query.constraints.end(); itt != ie; ++itt) {
  //             if (reqsetCompare.count(*itt))
  //       errs() << *itt << "\n";
  //       errs() << "\telts: " << IndependentElementSet(*itt) << "\n";
  //     }
  // }
  
  // **********************************************************

  //KLEE_DEBUG(
    std::set< ref<Expr> > reqset(result.begin(), result.end());
    errs() << "--\n";
    errs() << "Q: " << query.expr << "\n";
    errs() << "\telts: " << IndependentElementSet(query.expr) << "\n";
    int i = 0;
    for (ConstraintManager::const_iterator it = query.constraints.begin(),
        ie = query.constraints.end(); it != ie; ++it) {
      errs() << "C" << i++ << ": " << *it;
      errs() << " " << (reqset.count(*it) ? "(required)" : "(independent)") << "\n";
      errs() << "\telts: " << IndependentElementSet(*it) << "\n";
    }
    errs() << "elts closure: " << eltsClosure << "\n";
 //);

  stats::independentConstraints += result.size();
  stats::independentAllConstraints += query.constraints.size();
  return eltsClosure;
}


// Extracts which arrays are referenced from a particular independent set.  Examines both
// the actual known array accesses arr[1] plus the undetermined accesses arr[x].
static
void calculateArrayReferences(const IndependentElementSet & ie,
                              std::vector<const Array *> &returnVector){
  std::set<const Array*> thisSeen;
  for(std::map<const Array*, ::DenseSet<unsigned> >::const_iterator it = ie.elements.begin();
      it != ie.elements.end(); it ++){
    thisSeen.insert(it->first);
  }
  for(std::set<const Array *>::iterator it = ie.wholeObjects.begin();
      it != ie.wholeObjects.end(); it ++){
    thisSeen.insert(*it);
  }
  for(std::set<const Array *>::iterator it = thisSeen.begin(); it != thisSeen.end();
      it ++){
    returnVector.push_back(*it);
  }
}

class IndependentSolver : public SolverImpl {
private:
  Solver *solver;

public:
  IndependentSolver(Solver *_solver) 
    : solver(_solver) {}
  ~IndependentSolver() { delete solver; }

  bool computeTruth(const Query&, bool &isValid);
  bool computeValidity(const Query&, Solver::Validity &result);
  bool computeValue(const Query&, ref<Expr> &result);
  bool computeInitialValues(const Query& query,
                            const std::vector<const Array*> &objects,
                            std::vector< std::vector<unsigned char> > &values,
                            bool &hasSolution);
  SolverRunStatus getOperationStatusCode();
  char *getConstraintLog(const Query&);
  void setCoreSolverTimeout(time::Span timeout);
};
  
bool IndependentSolver::computeValidity(const Query& query,
                                        Solver::Validity &result) {
  TimerStatIncrementer t(stats::independentTime);
  std::vector< ref<Expr> > required;
  IndependentElementSet eltsClosure =
    getIndependentConstraints(query, required);
  ConstraintManager tmp(required);
  return solver->impl->computeValidity(Query(tmp, query.expr), 
                                       result);
}

bool IndependentSolver::computeTruth(const Query& query, bool &isValid) {
  TimerStatIncrementer t(stats::independentTime);
  std::vector< ref<Expr> > required;
  IndependentElementSet eltsClosure = 
    getIndependentConstraints(query, required);
  ConstraintManager tmp(required);
  return solver->impl->computeTruth(Query(tmp, query.expr), 
                                    isValid);
}

bool IndependentSolver::computeValue(const Query& query, ref<Expr> &result) {
  TimerStatIncrementer t(stats::independentTime);
  std::vector< ref<Expr> > required;
  IndependentElementSet eltsClosure = 
    getIndependentConstraints(query, required);
  ConstraintManager tmp(required);
  return solver->impl->computeValue(Query(tmp, query.expr), result);
}

// Helper function used only for assertions to make sure point created
// during computeInitialValues is in fact correct. The ``retMap`` is used
// in the case ``objects`` doesn't contain all the assignments needed.
bool assertCreatedPointEvaluatesToTrue(const Query &query,
                                       const std::vector<const Array*> &objects,
                                       std::vector< std::vector<unsigned char> > &values,
                                       std::map<const Array*, std::vector<unsigned char> > &retMap){
  // _allowFreeValues is set to true so that if there are missing bytes in the assigment
  // we will end up with a non ConstantExpr after evaluating the assignment and fail
  Assignment assign = Assignment(objects, values, /*_allowFreeValues=*/true);

  // Add any additional bindings.
  // The semantics of std::map should be to not insert a (key, value)
  // pair if it already exists so we should continue to use the assignment
  // from ``objects`` and ``values``.
  if (retMap.size() > 0)
    assign.bindings.insert(retMap.begin(), retMap.end());

  for(ConstraintManager::constraint_iterator it = query.constraints.begin();
      it != query.constraints.end(); ++it){
    ref<Expr> ret = assign.evaluate(*it);

    assert(isa<ConstantExpr>(ret) && "assignment evaluation did not result in constant");
    ref<ConstantExpr> evaluatedConstraint = dyn_cast<ConstantExpr>(ret);
    if(evaluatedConstraint->isFalse()){
      return false;
    }
  }
  ref<Expr> neg = Expr::createIsZero(query.expr);
  ref<Expr> q = assign.evaluate(neg);
  assert(isa<ConstantExpr>(q) && "assignment evaluation did not result in constant");
  return cast<ConstantExpr>(q)->isTrue();
}

bool IndependentSolver::computeInitialValues(const Query& query,
                                             const std::vector<const Array*> &objects,
                                             std::vector< std::vector<unsigned char> > &values,
                                             bool &hasSolution){
  TimerStatIncrementer t(stats::independentTime);
  // We assume the query has a solution except proven differently
  // This is important in case we don't have any constraints but
  // we need initial values for requested array objects.
  hasSolution = true;
  // FIXME: When we switch to C++11 this should be a std::unique_ptr so we don't need
  // to remember to manually call delete
  std::list<IndependentElementSet> *factors = getAllIndependentConstraintsSets(query);

  //Used to rearrange all of the answers into the correct order
  std::map<const Array*, std::vector<unsigned char> > retMap;
  for (std::list<IndependentElementSet>::iterator it = factors->begin();
       it != factors->end(); ++it) {
    std::vector<const Array*> arraysInFactor;
    calculateArrayReferences(*it, arraysInFactor);
    // Going to use this as the "fresh" expression for the Query() invocation below
    assert(it->exprs.size() >= 1 && "No null/empty factors");
    if (arraysInFactor.size() == 0){
      continue;
    }
    ConstraintManager tmp(it->exprs);
    std::vector<std::vector<unsigned char> > tempValues;
    if (!solver->impl->computeInitialValues(Query(tmp, ConstantExpr::alloc(0, Expr::Bool)),
                                            arraysInFactor, tempValues, hasSolution)){
      values.clear();
      delete factors;
      return false;
    } else if (!hasSolution){
      values.clear();
      delete factors;
      return true;
    } else {
      assert(tempValues.size() == arraysInFactor.size() &&
             "Should be equal number arrays and answers");
      for (unsigned i = 0; i < tempValues.size(); i++){
        if (retMap.count(arraysInFactor[i])){
          // We already have an array with some partially correct answers,
          // so we need to place the answers to the new query into the right
          // spot while avoiding the undetermined values also in the array
          std::vector<unsigned char> * tempPtr = &retMap[arraysInFactor[i]];
          assert(tempPtr->size() == tempValues[i].size() &&
                 "we're talking about the same array here");
          ::DenseSet<unsigned> * ds = &(it->elements[arraysInFactor[i]]);
          for (std::set<unsigned>::iterator it2 = ds->begin(); it2 != ds->end(); it2++){
            unsigned index = * it2;
            (* tempPtr)[index] = tempValues[i][index];
          }
        } else {
          // Dump all the new values into the array
          retMap[arraysInFactor[i]] = tempValues[i];
        }
      }
    }
  }
  for (std::vector<const Array *>::const_iterator it = objects.begin();
       it != objects.end(); it++){
    const Array * arr = * it;
    if (!retMap.count(arr)){
      // this means we have an array that is somehow related to the
      // constraint, but whose values aren't actually required to
      // satisfy the query.
      std::vector<unsigned char> ret(arr->size);
      values.push_back(ret);
    } else {
      values.push_back(retMap[arr]);
    }
  }
  assert(assertCreatedPointEvaluatesToTrue(query, objects, values, retMap) && "should satisfy the equation");
  delete factors;
  return true;
}

SolverImpl::SolverRunStatus IndependentSolver::getOperationStatusCode() {
  return solver->impl->getOperationStatusCode();      
}

char *IndependentSolver::getConstraintLog(const Query& query) {
  return solver->impl->getConstraintLog(query);
}

void IndependentSolver::setCoreSolverTimeout(time::Span timeout) {
  solver->impl->setCoreSolverTimeout(timeout);
}

Solver *klee::createIndependentSolver(Solver *s) {
  return new Solver(new IndependentSolver(s));
}

