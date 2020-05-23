#include "klee/Internal/Support/IndependentElementSet.h"
using namespace klee;
IndependentElementSet::IndependentElementSet(ref<Expr> e) {
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
    if (re->updates.root->isConstantArray() && re->updates.head.isNull())
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

void IndependentElementSet::print(llvm::raw_ostream &os) const {
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
bool IndependentElementSet::intersects(const IndependentElementSet &b) {
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
bool IndependentElementSet::add(const IndependentElementSet &b) {
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
