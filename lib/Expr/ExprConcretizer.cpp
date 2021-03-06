#include "klee/util/ExprConcretizer.h"
#include "klee/Internal/Support/ErrorHandling.h"

using namespace llvm;
using namespace klee;

void ExprConcretizer::cleanUp() {
  additionalConstraints.clear();
  foundExprs.clear();
  old2new.clear();
  farthestUpdates.clear();
}

ref<Expr> ExprConcretizer::getInitialValue
                            (const Array &mo, unsigned index) {
  std::pair<std::string, unsigned> k = {mo.name, index};
  if (concretizedInputs.find(k) != concretizedInputs.end()) {
    ref<Expr> rd = klee::ReadExpr::create(UpdateList(&mo, 0),
                          klee::ConstantExpr::alloc(index, mo.getDomain()));
    ref<Expr> cval = OracleEvaluator::getInitialValue(mo, index);
    ref<Expr> eq = klee::EqExpr::create(rd, cval);
    additionalConstraints.push_back(eq);
    return cval;
  }
  else {
    return klee::ReadExpr::create(UpdateList(&mo, 0),
                          klee::ConstantExpr::alloc(index, mo.getDomain()));
  }
}

ExprVisitor::Action ExprConcretizer::visitRead
                            (const ReadExpr &re) {
  std::vector<const UpdateNode *> updateNodeArray;
  const UpdateNode *originHead = nullptr;
  for (const UpdateNode *un = re.updates.head.get(); un; un = un->next.get()) {
    auto it = old2new.find(un);
    if (it == old2new.end()) {
      updateNodeArray.push_back(un);
    }
    else {
      originHead = it->second;
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
    // FIXME: on going debate.
    // Note that there should be no divergence in any updatelist.
    // So either:
    //   1. originHead is the farthest and we are extending the farthest (updateNodeArray non-empty)
    //   2. originHead is not the farthest. we meet a ReadExpr referencing a shorter updateList (an older version of memory state of the root array)
    if (ul->head.get() != originHead && !updateNodeArray.empty()) {
      klee_warning("farthest != originHead but updateNodeArray is not empty. violating case 2");
    }
  }

  for (auto it = updateNodeArray.rbegin(); it != updateNodeArray.rend(); ++it) {
    const UpdateNode *un = *it;
    updateNodeArray.pop_back();
    auto idx = visit(un->index);
    auto val = visit(un->value);
    // FIXME: on going debate
    // Due to recursion, the updateNodeArray we are processing here might already be processed inside recursion.
    // We currently just check if such repeated updates happen to old2new map.
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
    old2new.insert({un, ul->head.get()});
  }

  ref<Expr> idx = visit(re.index);

  if (klee::ConstantExpr *CE = dyn_cast<klee::ConstantExpr>(idx)) {
    uint64_t index = CE->getZExtValue();

    /* Iterate on the update list and search for a match,
     * if there's no match we return a read instead */
    for (UpdateNode *un = ul->head.get(); un; un=un->next.get()) {
      if (ConstantExpr *ci = dyn_cast<ConstantExpr>(un->index)) {
        if (ci->getZExtValue() == index)
          return Action::changeTo(un->value);
      }
      else {
        // we meet a symbolic index in the update list, stop matching concrete update nodes
        // return a ReadExpr from this symbolic update node instead.
        return Action::changeTo(ReadExpr::create(UpdateList(ul->root, un),
                        ConstantExpr::alloc(index, ul->root->getDomain())));
      }
    }

    // we can not find a matching concrete update, but all updates are concrete
    // let us match the root Array behind the update list for a concrete match.
    if (ul->root->isConstantArray()) {
      if (index < ul->root->size) {
        return Action::changeTo(ul->root->constantValues[index]);
      }
      else {
        klee_warning("out of bound access (index %lu) to constant Array %s (size %u)", index, ul->root->name.c_str(), ul->root->size);
      }
    }

    // there is no symbolic update but the root Array is symbolic.
    // we want to query its initial value, additonal concretized value hold
    // by this evaluator could be applied here.
    return Action::changeTo(getInitialValue(*ul->root, index));
  } else {
    // symbolic index, there seems to be nothing we can do
    return Action::changeTo(ReadExpr::create(UpdateList(ul->root, ul->head), idx));
  }
}

ExprVisitor::Action ExprConcretizer::visitExpr(const Expr &e) {
  for (auto it = concretizedExprs.begin(), ie = concretizedExprs.end();
        it != ie; it++) {
    if (e == *it->first.get()) {
      foundExprs[it->first] = true;
      ref<Expr> val = ConstantExpr::alloc(it->second, e.getWidth());
      return Action::changeTo(val);
    }
  }
  return Action::doChildren();
}

ExprVisitor::Action ExprConcretizer::visitExprPost(const Expr &e) {
  for (auto it = concretizedExprs.begin(), ie = concretizedExprs.end();
        it != ie; it++) {
    if (e == *it->first.get()) {
      foundExprs[it->first] = true;
      ref<Expr> val = ConstantExpr::alloc(it->second, e.getWidth());
      return Action::changeTo(val);
    }
  }
  return Action::doChildren();
}

void ExprConcretizer::addConcretizedInputValue
                            (std::string arrayName, unsigned index) {
  if (arrayname2idx.find(arrayName) == arrayname2idx.end()) {
    klee_warning("Trying to concretize non-exist symbolic obj: %s", arrayName.c_str());
  }
  std::pair<std::string, unsigned> k = {arrayName, index};
  assert(concretizedInputs.find(k) == concretizedInputs.end());
  concretizedInputs.insert(k);
}

void ExprConcretizer::addConcretizedExprValue
                            (ref<Expr> e, uint64_t val) {
  assert(concretizedExprs.find(e) == concretizedExprs.end());
  assert(foundExprs.find(e) == foundExprs.end());
  concretizedExprs.insert({e, val});
  foundExprs.insert({e, false});
}

Constraints_ty ExprConcretizer::doEvaluate(
    const Constraints_ty::const_iterator ib,
    const Constraints_ty::const_iterator ie) {

  Constraints_ty newCm;

  /* for all constraints:
   * 1. replace all exprs in concretizedExprs to a concrete value
   * 2. replace all inputs in concretizedInputs to a concrete value */
  for (auto it = ib; it != ie; it++) {
    const ref<Expr> &e = *it;
    auto newExpr = visit(e);
    newCm.insert(newExpr);
  }

  for (auto it = additionalConstraints.begin(),
          ie = additionalConstraints.end(); it != ie; it++) {
    newCm.insert(*it);
  }

  /* after replacing a concretized Expr, we need to add a new
   * constraint */
  for (auto it = foundExprs.begin(), ie = foundExprs.end();
        it != ie; it++) {
    if (it->second) {
      ref<Expr> ori = it->first;
      uint64_t val = concretizedExprs[ori];
      ref<Expr> v = ConstantExpr::create(val, ori->getWidth());
      ref<Expr> eq = EqExpr::create(v, ori);
      newCm.insert(eq);
    }
  }

  cleanUp();

  return newCm;
}

Constraints_ty ExprConcretizer::evaluate(const Constraints_ty &constraints) {
  return doEvaluate(constraints.begin(), constraints.end());
}

/*
 * IndirectReadDepthCalculator
 */
int IndirectReadDepthCalculator::getLevel(const ref<Expr> &e) {
  auto it = depthStore.find(e.get());

  if (it == depthStore.end()) {
    depthStore.insert(std::make_pair(e.get(), -1));
    return -1;
  }
  else {
    return it->second;
  }
}

void IndirectReadDepthCalculator::putLevel(const ref<Expr> &e, int level) {
  auto it = depthStore.find(e.get());
  assert(it != depthStore.end());

  it->second = level;
}

int IndirectReadDepthCalculator::assignDepth(const ref<Expr> &e, int readLevel) {
  int initialLevel = getLevel(e);

  if (initialLevel >= readLevel) {
    return initialLevel;
  }
  putLevel(e, readLevel);

  // m means the maximum indirect depth which is maintained recursively
  int m = readLevel;
  if (ReadExpr *RE = dyn_cast<ReadExpr>(e)) {
    updateMaxLevelfromKid(RE->index, readLevel + 1, m);
    const UpdateNode *un = RE->updates.head.get();
    std::vector<const UpdateNode *> un_vec;
    while (un) {
      un_vec.push_back(un);
      updateMaxLevelfromKid(un->index, readLevel + 1, m);
      updateMaxLevelfromKid(un->value, readLevel, m);
      /*
       * Indirect depth of an UpdateNode depends on its parent ReadExpr
       */
      auto insert_pair = depthStoreUNode.insert(std::make_pair(un, readLevel));
      if (insert_pair.second) {
        // insert successfully, meaning see this UNode first time
        // do nothing
        ;
      }
      else {
        decltype(depthStoreUNode)::iterator &existed = insert_pair.first;
        if (readLevel > existed->second) {
          existed->second = readLevel;
        }
      }
      un = un->next.get();
    }

    if (!RE->index.isNull() && (RE->index->getKind() == Expr::Constant) &&
        RE->updates.head.isNull()) {
      lastLevelReads.insert(RE);
    }
  }
  else {
    for (unsigned int i=0; i < e->getNumKids(); ++i) {
      updateMaxLevelfromKid(e->getKid(i), readLevel, m);
    }
  }
  return m;
}

IndirectReadDepthCalculator::IndirectReadDepthCalculator(
    const Constraints_ty &constraints) {
  maxLevel = 0;
  for (const ref<Expr> &e: constraints) {
    int m = assignDepth(e, 0);
    if (m > maxLevel) maxLevel = m;
  }
}

IndirectReadDepthCalculator::IndirectReadDepthCalculator(
    const expr::QueryCommand &QC) {
  maxLevel = 0;
  for (const ref<Expr> &e: QC.Constraints) {
    int m = assignDepth(e, 0);
    if (m > maxLevel) maxLevel = m;
  }
  for (const ref<Expr> &e: QC.Values) {
    int m = assignDepth(e, 0);
    if (m > maxLevel) maxLevel = m;
  }
}


int IndirectReadDepthCalculator::query(const Expr *e) const {
  auto it = depthStore.find(e);
  if (it == depthStore.end()) {
    return -1;
  }
  else {
    return it->second;
  }
}

