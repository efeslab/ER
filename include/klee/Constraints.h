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

#include "klee/Expr.h"
#include "klee/util/ExprUtil.h"
#include "klee/util/ExprHashMap.h"
#include <unordered_map>
#include <unordered_set>

using namespace klee;
using namespace llvm;

// Originally locate at IndependentSolver.cpp
template<class T>
class DenseSet {
  typedef std::set<T> set_ty;
  set_ty s;

public:
  DenseSet() {}

  void add(T x) {
    s.insert(x);
  }
  void add(T start, T end) {
    for (; start<end; start++)
      s.insert(start);
  }

  // returns true iff set is changed by addition
  bool add(const DenseSet &b) {
    bool modified = false;
    for (typename set_ty::const_iterator it = b.s.begin(), ie = b.s.end(); 
         it != ie; ++it) {
      if (modified || !s.count(*it)) {
        modified = true;
        s.insert(*it);
      }
    }
    return modified;
  }

  bool intersects(const DenseSet &b) {
    for (typename set_ty::iterator it = s.begin(), ie = s.end(); 
         it != ie; ++it)
      if (b.s.count(*it))
        return true;
    return false;
  }

  std::set<unsigned>::iterator begin(){
    return s.begin();
  }

  std::set<unsigned>::iterator end(){
    return s.end();
  }

  void print(llvm::raw_ostream &os) const {
    bool first = true;
    os << "{";
    for (typename set_ty::iterator it = s.begin(), ie = s.end(); 
         it != ie; ++it) {
      if (first) {
        first = false;
      } else {
        os << ",";
      }
      os << *it;
    }
    os << "}";
  }
};

template <class T>
inline llvm::raw_ostream &operator<<(llvm::raw_ostream &os,
                                     const ::DenseSet<T> &dis) {
  dis.print(os);
  return os;
}

class IndependentElementSet {
public:
  typedef std::map<const Array*, ::DenseSet<unsigned> > elements_ty;
  elements_ty elements;                 // Represents individual elements of array accesses (arr[1])
  std::set<const Array*> wholeObjects;  // Represents symbolically accessed arrays (arr[x])
  std::vector<ref<Expr> > exprs;        // All expressions that are associated with this factor
                                        // Although order doesn't matter, we use a vector to match
                                        // the ConstraintManager constructor that will eventually
                                        // be invoked.

  IndependentElementSet() {}
  IndependentElementSet(ref<Expr> e) {
    exprs.push_back(e);
    // Track all reads in the program.  Determines whether reads are
    // concrete or symbolic.  If they are symbolic, "collapses" array
    // by adding it to wholeObjects.  Otherwise, creates a mapping of
    // the form Map<array, set<index>> which tracks which parts of the
    // array are being accessed.
    std::vector< ref<ReadExpr> > reads;
    findReads(e, /* visitUpdates= */ true, reads);
    for (unsigned i = 0; i != reads.size(); ++i) {
      ReadExpr *re = reads[i].get();
      const Array *array = re->updates.root;
      
      // Reads of a constant array don't alias.
      if (re->updates.root->isConstantArray() &&
          !re->updates.head)
        continue;

      if (!wholeObjects.count(array)) {
        if (ConstantExpr *CE = dyn_cast<ConstantExpr>(re->index)) {
          // if index constant, then add to set of constraints operating
          // on that array (actually, don't add constraint, just set index)
          ::DenseSet<unsigned> &dis = elements[array];
          dis.add((unsigned) CE->getZExtValue(32));
        } else {
          elements_ty::iterator it2 = elements.find(array);
          if (it2!=elements.end())
            elements.erase(it2);
          wholeObjects.insert(array);
        }
      }
    }
  }
  IndependentElementSet(const IndependentElementSet &ies) : 
    elements(ies.elements),
    wholeObjects(ies.wholeObjects),
    exprs(ies.exprs) {}

  IndependentElementSet &operator=(const IndependentElementSet &ies) {
    elements = ies.elements;
    wholeObjects = ies.wholeObjects;
    exprs = ies.exprs;
    return *this;
  }

  void print(llvm::raw_ostream &os) const {
    os << "{";
    bool first = true;
    for (std::set<const Array*>::iterator it = wholeObjects.begin(), 
           ie = wholeObjects.end(); it != ie; ++it) {
      const Array *array = *it;

      if (first) {
        first = false;
      } else {
        os << ", ";
      }

      os << "MO" << array->name;
    }
    for (elements_ty::const_iterator it = elements.begin(), ie = elements.end();
         it != ie; ++it) {
      const Array *array = it->first;
      const ::DenseSet<unsigned> &dis = it->second;

      if (first) {
        first = false;
      } else {
        os << ", ";
      }

      os << "MO" << array->name << " : " << dis;
    }
    os << "}";
  }

  // more efficient when this is the smaller set
  bool intersects(const IndependentElementSet &b) {
    // If there are any symbolic arrays in our query that b accesses
    for (std::set<const Array*>::iterator it = wholeObjects.begin(), 
           ie = wholeObjects.end(); it != ie; ++it) {
      const Array *array = *it;
      if (b.wholeObjects.count(array) || 
          b.elements.find(array) != b.elements.end())
        return true;
    }
    for (elements_ty::iterator it = elements.begin(), ie = elements.end();
         it != ie; ++it) {
      const Array *array = it->first;
      // if the array we access is symbolic in b
      if (b.wholeObjects.count(array))
        return true;
      elements_ty::const_iterator it2 = b.elements.find(array);
      // if any of the elements we access are also accessed by b
      if (it2 != b.elements.end()) {
        if (it->second.intersects(it2->second))
          return true;
      }
    }
    return false;
  }

  // returns true iff set is changed by addition
  bool add(const IndependentElementSet &b) {
    for(unsigned i = 0; i < b.exprs.size(); i ++){
      ref<Expr> expr = b.exprs[i];
      exprs.push_back(expr);
    }

    bool modified = false;
    for (std::set<const Array*>::const_iterator it = b.wholeObjects.begin(), 
           ie = b.wholeObjects.end(); it != ie; ++it) {
      const Array *array = *it;
      elements_ty::iterator it2 = elements.find(array);
      if (it2!=elements.end()) {
        modified = true;
        elements.erase(it2);
        wholeObjects.insert(array);
      } else {
        if (!wholeObjects.count(array)) {
          modified = true;
          wholeObjects.insert(array);
        }
      }
    }
    for (elements_ty::const_iterator it = b.elements.begin(), 
           ie = b.elements.end(); it != ie; ++it) {
      const Array *array = it->first;
      if (!wholeObjects.count(array)) {
        elements_ty::iterator it2 = elements.find(array);
        if (it2==elements.end()) {
          modified = true;
          elements.insert(*it);
        } else {
          // Now need to see if there are any (z=?)'s
          if (it2->second.add(it->second))
            modified = true;
        }
      }
    }
    return modified;
  }
};

inline llvm::raw_ostream &operator<<(llvm::raw_ostream &os,
                                     const IndependentElementSet &ies) {
  ies.print(os);
  return os;
}

// FIXME: Currently we use ConstraintManager for two things: to pass
// sets of constraints around, and to optimize constraints. We should
// move the first usage into a separate data structure
// (ConstraintSet?) which ConstraintManager could embed if it likes.
namespace klee {

class ExprVisitor;
  
class ConstraintManager {
public:
  typedef std::vector< ref<Expr> > constraints_ty;
  typedef constraints_ty::iterator iterator;
  typedef constraints_ty::const_iterator const_iterator;

  ConstraintManager() {}

  // create from constraints with no optimization
  explicit
  ConstraintManager(const std::vector< ref<Expr> > &_constraints) :
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

  ConstraintManager(const ConstraintManager &cs) : constraints(cs.constraints) {
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
  ~ConstraintManager() {
    // Here we assume every IndependentElementSet point in representative also exist in factors.
    for (auto it = factors.begin(); it != factors.end(); it++) {
      delete(*it);
    }
  }

  typedef std::vector< ref<Expr> >::const_iterator constraint_iterator;
  typedef std::unordered_set<IndependentElementSet*>::const_iterator factor_iterator;

  // given a constraint which is known to be valid, attempt to 
  // simplify the existing constraint set
  void simplifyForValidConstraint(ref<Expr> e);

  ref<Expr> simplifyExpr(ref<Expr> e) const;

  void addConstraint(ref<Expr> e);
  
  bool empty() const {
    return constraints.empty();
  }
  ref<Expr> back() const {
    return constraints.back();
  }
  constraint_iterator begin() const {
    return constraints.begin();
  }
  constraint_iterator end() const {
    return constraints.end();
  }
  
  factor_iterator factor_begin() const {
    return factors.begin();
  }
  
  factor_iterator factor_end() const {
    return factors.end();
  }
  
  size_t size() const {
    return constraints.size();
  }

  bool operator==(const ConstraintManager &other) const {
    return constraints == other.constraints;
  }
  
private:
  std::vector< ref<Expr> > constraints;
  std::vector< ref<Expr> > old;
  ExprHashMap< IndependentElementSet*> representative;
  std::vector<ref<Expr>> deleteConstraints;
  std::vector<ref<Expr>> addedConstraints;
  std::unordered_set<IndependentElementSet*> factors;

  // returns true iff the constraints were modified
  bool rewriteConstraints(ExprVisitor &visitor);

  bool addConstraintInternal(ref<Expr> e);
  
  void updateIndependentSet();
  
  void checkConstraintChange();
  
  void updateDelete();
};

}

#endif /* KLEE_CONSTRAINTS_H */

