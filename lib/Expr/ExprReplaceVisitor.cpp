#include "klee/Expr/ExprReplaceVisitor.h"
using namespace klee;
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
          newUN = replacedUN_it->second;
        }
        visitedUN[n] = newUN;
        nextUN = newUN;
      } else {
        visitedUN[n] = n;
        nextUN = n;
      }
      un_worklist.pop_back();
      local_visited.pop_back();
    }
  }
  return nextUN;
}

#ifdef DEBUG_EQUIV_RELEASE
// to embed debugging symbols of UNMap_ty for debugging
// replacedUN and visitedUN
template class std::unordered_map<ref<UpdateNode>, ref<UpdateNode>, klee::util::RefHash<UpdateNode>, klee::util::RefCmp<UpdateNode>>;
#endif
