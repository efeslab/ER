#include "ExprConcretizer.h"
#include "OracleEvaluator.h"

#include "klee/Common.h"
#include "klee/Config/Version.h"
#include "klee/Expr.h"
#include "klee/Internal/Module/KInstruction.h"
#include "klee/util/ExprPPrinter.h"
#include "klee/util/ExprUtil.h"
#include "klee/util/ExprVisitor.h"

#include "llvm/IR/Instructions.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Format.h"

#include <algorithm>
#include <cassert>
#include <cerrno>
#include <cxxabi.h>
#include <fstream>
#include <iomanip>
#include <iosfwd>
#include <sstream>
#include <string>
#include <sys/mman.h>
#include <vector>

using namespace llvm;
using namespace klee;

ref<Expr> ArrayConcretizationEvaluator::getInitialValue
                            (const Array &mo, unsigned index) {
  std::pair<std::string, unsigned> k = {mo.name, index};
  if (concretizedValues.find(k) != concretizedValues.end()) {
    return OracleEvaluator::getInitialValue(mo, index);
  }
  else {
    return klee::ReadExpr::create(UpdateList(&mo, 0),
                          klee::ConstantExpr::alloc(index, mo.getDomain()));
  }
}

ExprVisitor::Action ArrayConcretizationEvaluator::visitRead
                            (const ReadExpr &re) {
  std::vector<const UpdateNode *> updateNodeArray;
  const UpdateNode *originHead = nullptr;
  for (const UpdateNode *un = re.updates.head; un; un = un->next) {
    if (old2new.find(un) == old2new.end()) {
      updateNodeArray.push_back(un);
    }
    else {
      originHead = old2new[un];
      break;
    }
  }

  auto farthest = farthestUpdates.find(re.updates.root->name);
  UpdateList *ul = nullptr;
  if (farthest == farthestUpdates.end()) {
    assert(originHead == nullptr);
    ul = new UpdateList(re.updates.root, nullptr);
    farthestUpdates.insert({re.updates.root->name, ul});
  }
  else {
    ul = farthest->second;
    if (ul->head && originHead)
      assert(ul->head->getSize() >= originHead->getSize());
  }

  for (int i = updateNodeArray.size()-1; i >= 0; i--) {
    const UpdateNode *un = updateNodeArray[i];
    updateNodeArray.pop_back();
    auto idx = visit(un->index);
    auto val = visit(un->value);
    assert(old2new.find(un) == old2new.end());
    ul->extend(idx, val, Expr::FLAG_INTERNAL, un->kinst);
    assert(old2new.find(un) == old2new.end());
#if 0
    llvm::errs() << format("%#lx", (uint64_t)un)
            <<  ":{" << format("%#lx", (uint64_t)un->index.get())
            << ", " << format("%#lx", (uint64_t)un->value.get()) << "}"
            << " ==> " << format("%#lx", (uint64_t)ul->head)
            << ":{" << format("%#lx", (uint64_t)ul->head->index.get())
            << ", " << format("%#lx", (uint64_t)ul->head->value.get()) << "}\n";
#endif
    old2new.insert({un, ul->head});
  }

  ref<Expr> v = visit(re.index);

  if (klee::ConstantExpr *CE = dyn_cast<klee::ConstantExpr>(v)) {
    uint64_t index = CE->getZExtValue();

    /* Iterate on the update list and search for a match,
     * if there's no match we return a read instead */
    for (const UpdateNode *un = ul->head; un; un=un->next) {
      if (ConstantExpr *ci = dyn_cast<ConstantExpr>(un->index)) {
        if (ci->getZExtValue() == index)
          return Action::changeTo(un->value);
      }
      else {
        return Action::changeTo(ReadExpr::create(UpdateList(ul->root, un),
                        ConstantExpr::alloc(index, ul->root->getDomain())));
      }
    }

    if (ul->root->isConstantArray() && index < ul->root->size)
      return Action::changeTo(ul->root->constantValues[index]);

    return Action::changeTo(getInitialValue(*ul->root, index));
  } else {
      return Action::changeTo(ReadExpr::create(UpdateList(ul->root, ul->head), v));
  }
}

void ArrayConcretizationEvaluator::addConcretizedValue
                            (std::string arrayName, unsigned index) {
  std::pair<std::string, unsigned> k = {arrayName, index};
  if (concretizedValues.find(k) == concretizedValues.end()) {
    concretizedValues.insert(k);
  }
}

ConstraintManager ArrayConcretizationEvaluator::evaluate(ConstraintManager &cm) {
  std::vector<ref<Expr>> newCm;
  for (auto it = cm.begin(), ie = cm.end(); it != ie; it++) {
    const ref<Expr> &e = *it;
    auto newExpr = visit(e);
    newCm.push_back(newExpr);
  }
  return ConstraintManager(newCm);
}

int IndirectReadDepthCalculator::getLevel(const ref<Expr> &e) {
  auto it = depthStore.find(e);

  if (it == depthStore.end()) {
    depthStore.insert(std::make_pair(e, -1));
    return -1;
  }
  else {
    return it->second;
  }
}

void IndirectReadDepthCalculator::putLevel(const ref<Expr> &e, int level) {
  auto it = depthStore.find(e);
  assert(it != depthStore.end());

  depthStore[e] = level;
}

int IndirectReadDepthCalculator::assignDepth(const ref<Expr> &e, int readLevel) {
  int initialLevel = getLevel(e);

  if (initialLevel >= readLevel) {
    return initialLevel;
  }
  putLevel(e, readLevel);

  int m = readLevel;
  bool shouldIncrease = false;
  if (e->getKind() == Expr::Read) {
    const ref<ReadExpr> &re = dyn_cast<ReadExpr>(e);
    if (re->index->getKind() != Expr::Constant) {
      shouldIncrease = true;
    }
    else {
      for (auto un = re->updates.head; un; un = un->next) {
        if (un->index->getKind() != Expr::Constant) {
          shouldIncrease = true;
          break;
        }
      }
    }
  }

  int nkids = e->getNumKids();
  for (int i = 0; i < nkids; i++) {
    ref<Expr> kid = e->getKid(i);
    int m1;
    if (shouldIncrease) {
      m1 = assignDepth(kid, readLevel+1);
    }
    else {
      m1 = assignDepth(kid, readLevel);
    }
    if (m1 > m) m = m1;
  }

  if (e->getKind() == Expr::Read) {
    const ref<ReadExpr> &re = dyn_cast<ReadExpr>(e);
    const UpdateNode *un = re->updates.head;
    for (; un; un = un->next) {
      int m2 = assignDepth(un->index, readLevel+1);
      int m3 = assignDepth(un->value, readLevel);
      if (m2 > m) m = m2;
      if (m3 > m) m = m3;
    }
  }

  if (e->getKind() == Expr::Read) {
    const ref<ReadExpr> &re = dyn_cast<ReadExpr>(e);
    if (re->index->getKind() == Expr::Constant &&
            re->updates.head == nullptr) {
      auto it = lastLevelReads.find(e);
      if (it == lastLevelReads.end()) {
        lastLevelReads.insert(e);
      }
    }
  }

  return m;
}

IndirectReadDepthCalculator::IndirectReadDepthCalculator(ConstraintManager &cm) {
  maxLevel = 0;
  for (auto it = cm.begin(), ie = cm.end(); it != ie; it++) {
    const ref<Expr> &e = *it;
    int m = assignDepth(e, 0);
    if (m > maxLevel) maxLevel = m;
  }
}

int IndirectReadDepthCalculator::getMax() {
  return maxLevel;
}

std::set<ref<Expr>>& IndirectReadDepthCalculator::getLastLevelReads() {
  return lastLevelReads;
}

int IndirectReadDepthCalculator::query(const ref<Expr> &e) {
  auto it = depthStore.find(e);
  if (it == depthStore.end()) {
    return -1;
  }
  else {
    return it->second;
  }
}


