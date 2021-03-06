//===-- Updates.cpp -------------------------------------------------------===//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "klee/Expr/Expr.h"
//#include "klee/Internal/Module/KInstruction.h"

#include <cassert>

using namespace klee;

///
UpdateNode::UNEquivSet UpdateNode::UNequivs;
static unsigned kinstMissCounter = 0;
UpdateNode::UpdateNode(const ref<UpdateNode> &_next, const ref<Expr> &_index,
                       const ref<Expr> &_value, uint64_t _flags,
                       KInstruction *_kinst)
    : next(_next), index(_index), value(_value), flags(_flags), kinst(_kinst) {
  // FIXME: What we need to check here instead is that _value is of the same width 
  // as the range of the array that the update node is part of.
  /*
  assert(_value->getWidth() == Expr::Int8 && 
         "Update value should be 8-bit wide.");
  */
  computeHash();

  if (_flags == Expr::FLAG_INTERNAL && _kinst == nullptr) {
      kinstMissCounter++;
  }

  size = next.isNull() ? 1 : 1 + next->size;
}

extern "C" void vc_DeleteExpr(void*);

int UpdateNode::compare(const UpdateNode &b) const {
  if (this == &b) return 0;

  // prepare ordered key for UN equiv cache query
  const UpdateNode *ap, *bp;
  if (this < &b) {
    ap = this; bp = &b;
  } else {
    ap = &b; bp = this;
  }

  // check cache
  if (UNequivs.count(
          std::make_pair(ref<const UpdateNode>(ap), ref<const UpdateNode>(bp))))
    return 0;

  if (hashValue != b.hashValue) {
    return (hashValue < b.hashValue) ? -1 : 1;
  }
  if (int cmp = index.compare(b.index))
    return cmp;
  if (int cmp = value.compare(b.value))
    return cmp;
  if (getSize() < b.getSize())
    return -1;
  else if (getSize() > b.getSize())
    return 1;
  else {
    int res = next.compare(b.next);
    if (res == 0) {
      UNequivs.insert(
          std::make_pair(ref<const UpdateNode>(ap), ref<const UpdateNode>(bp)));
    }
    return res;
  }
}

unsigned UpdateNode::computeHash() {
  hashValue = 0;
  if (!index.isNull()) {
    hashValue = index->hash();
  }
  if (!value.isNull()) {
    hashValue ^= value->hash();
  }
  if (!next.isNull())
    hashValue ^= next->hash();
  return hashValue;
}

///

UpdateList::UpdateList(const Array *_root, const ref<UpdateNode> &_head)
    : root(_root), head(_head) {}

void UpdateList::extend(const ref<Expr> &index, const ref<Expr> &value,
                uint64_t flags, KInstruction *kinst) {
  
  if (root) {
    assert(root->getDomain() == index->getWidth());
    assert(root->getRange() == value->getWidth());
  }

  head = new UpdateNode(head, index, value, flags, kinst);
}

int UpdateList::compare(const UpdateList &b) const {
  if (root->name != b.root->name)
    return root->name < b.root->name ? -1 : 1;

  // Check the root itself in case we have separate objects with the
  // same name.
  if (root != b.root)
    return root < b.root ? -1 : 1;

  if (getSize() < b.getSize())
    return -1;
  else if (getSize() > b.getSize())
    return 1;
  else if (head == b.head)
    return 0;
  else
    return head.compare(b.head);
}

unsigned UpdateList::hash() const {
  unsigned res = 0;
  for (unsigned i = 0, e = root->name.size(); i != e; ++i)
    res = (res * Expr::MAGIC_HASH_CONSTANT) + root->name[i];
  if (head.get())
    res ^= head->hash();
  return res;
}

void debugDumpUpdateNodes(const UpdateNode *un) {
  llvm::errs() << '[';
  while (un) {
    un->index->print(llvm::errs());
    llvm::errs() << '=';
    un->value->print(llvm::errs());
    llvm::errs() << ", ";
    un = un->next.get();
  }
  llvm::errs() << "]\n";
}
