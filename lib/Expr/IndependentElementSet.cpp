#include "klee/Internal/Support/IndependentElementSet.h"
using namespace klee;
IndependentElementSet::IndependentElementSet(ref<Expr> e) {
  exprs.insert(e);
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
bool IndependentElementSet::intersects(const IndependentElementSet &b) const {
  // If there are any symbolic arrays in our query that b accesses
  const std::set<const Array*> *smallerWholeObjects = nullptr;
  const std::set<const Array*> *largerWholeObjects = nullptr;
  const elements_ty *elementsWithlargerWholeObjects = nullptr;
  if (wholeObjects.size() < b.wholeObjects.size()) {
    smallerWholeObjects = &wholeObjects;
    largerWholeObjects = &(b.wholeObjects);
    elementsWithlargerWholeObjects = &(b.elements);
  } else {
    smallerWholeObjects = &(b.wholeObjects);
    largerWholeObjects = &wholeObjects;
    elementsWithlargerWholeObjects = &elements;
  }
  for (const Array* array: *smallerWholeObjects) {
    if (largerWholeObjects->count(array) ||
        (elementsWithlargerWholeObjects->find(array) !=
         elementsWithlargerWholeObjects->end()))
      return true;
  }

  // check whether concrete array accesses overlap
  const elements_ty *smallerElements = nullptr;
  const elements_ty *largerElements = nullptr;
  const std::set<const Array*> *wholeObjectsWithlargerElements = nullptr;
  if (elements.size() < b.elements.size()) {
    smallerElements = &elements;
    largerElements = &(b.elements);
    wholeObjectsWithlargerElements = &(b.wholeObjects);
  } else {
    smallerElements = &(b.elements);
    largerElements = &elements;
    wholeObjectsWithlargerElements = &wholeObjects;
  }
  for (const elements_ty::value_type &it : *smallerElements) {
    const Array *array = it.first;
    // if the array we access is symbolic in b
    if (wholeObjectsWithlargerElements->count(array))
      return true;
    elements_ty::const_iterator it2 = largerElements->find(array);
    // if any of the elements we access are also accessed by b
    if (it2 != largerElements->end()) {
      if (it.second.intersects(it2->second))
        return true;
    }
  }
  return false;
}

// returns true iff set is changed by addition
bool IndependentElementSet::add(const IndependentElementSet &b) {
  exprs.insert(b.exprs.begin(), b.exprs.end());

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
