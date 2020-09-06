#include "klee/Expr/ExprReplaceVisitor.h"
using namespace klee;
//UpdateNodeEquivSet klee::equivUN;
//uint64_t klee::UNCacheHit;
//uint64_t klee::UNCacheMiss;
ref<UpdateNode>
ExprReplaceVisitorBase::visitUpdateNode(const ref<UpdateNode> &un) {
  unsigned un_len = un->getSize();
  std::vector<ref<UpdateNode>> un_worklist;
  std::vector<bool> local_visited;
  ref<UpdateNode> nextUN;
  un_worklist.reserve(un_len);
  local_visited.reserve(un_len);
  un_worklist.push_back(un);
  local_visited.push_back(false);
  while (!un_worklist.empty()) {
    ref<UpdateNode> &n = un_worklist.back();
    if (!local_visited.back()) {
      // unvisited
      auto find_it = visitedUN.find(n);
      if (find_it != visitedUN.end()) {
        // replace current UpdateNode using visitedUN cache
        local_visited.pop_back();
        un_worklist.pop_back();
        nextUN = find_it->second;
      } else {
        local_visited.back() = true;
        if (n->next.isNull()) {
          nextUN = nullptr;
        } else {
          un_worklist.push_back(n->next);
          local_visited.push_back(false);
        }
      }
    } else {
      // visited, collect results from nextUN
      ref<Expr> index;
      ref<Expr> value;
      if (isa<ConstantExpr>(n->index)) {
        index = n->index;
      } else {
        index = visit(n->index);
      }
      if (isa<ConstantExpr>(n->value)) {
        value = n->value;
      } else {
        value = visit(n->value);
      }
      if ((index.get() != n->index.get()) ||
          (value.get() != n->value.get()) || (nextUN.get() != n->next.get())) {
        ref<UpdateNode> newUN = new UpdateNode(nextUN, index, value, n->flags, n->kinst);
        UNMap_ty::iterator replacedUN_it;
        bool unseen;
        std::tie(replacedUN_it, unseen) =
            replacedUN.insert(std::make_pair(newUN, newUN));
        if (!unseen) {
          // A previously seen UN (found in the replacedUN) is going to replace
          // newUN (just allocated above)
          replacedUNBuffer.push_back(newUN);
          newUN = replacedUN_it->second;
        }
        visitedUN[n] = newUN;
        nextUN = newUN;
        // newUN is going to replace n
        replacedUNBuffer.push_back(n);
      } else {
        visitedUN[n] = n;
        nextUN = n;
      }
      un_worklist.pop_back();
      local_visited.pop_back();
    }
  }
  return nextUN;
  /* 
  // TODO: In recursive mode, should I repeatedly search visited Exprs, so
  // that chains of replacement can resolved quicker.
  auto find_it = visitedUN.find(un);
  if (find_it != visitedUN.end()) {
    assert(nextUN == find_it->second);
    return find_it->second;
  }
  ref<UpdateNode> next;
  if (!un->next.isNull()) {
    next = visitUpdateNode(un->next);
  }
  const ref<Expr> index = visit(un->index);
  const ref<Expr> value = visit(un->value);
  if (index.get() != un->index.get() || value.get() != un->value.get() ||
      next.get() != un->next.get()) {
    ref<UpdateNode> newUN =
        new UpdateNode(next, index, value, un->flags, un->kinst);
    UNMap_ty::iterator replacedUN_it;
    bool unseen;
    std::tie(replacedUN_it, unseen) =
        replacedUN.insert(std::make_pair(newUN, newUN));
    if (!unseen) {
      newUN = replacedUN_it->second;
    }
    visitedUN[un] = newUN;
    assert(nextUN == newUN);
    return newUN;
  }
  visitedUN[un] = un;
  assert(nextUN == un);
  return un;
  */
}
