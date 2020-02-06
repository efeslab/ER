#include "ExprInPlaceTransformation.h"
using namespace klee;
static Expr *under_processing_expr = (Expr*)(0x1);
static const UpdateNode *under_processing_un = (const UpdateNode*)(0x1);

ExprInPlaceTransformer::ExprInPlaceTransformer(const Constraints_ty &_constraints,
    Constraints_ty &out_constraints): constraints(_constraints) {
  out_constraints.clear();
  for (const ref<Expr> &e: constraints) {
    visitDFS(e.get());
    out_constraints.push_back(popKidExpr());
  }
}
// TODO UpdateNode* memory leak
void ExprInPlaceTransformer::visitDFS(Expr *e) {
  expr_worklist.push_back(e);
  while (!expr_worklist.empty()) {
    WorkListEntry &entry = expr_worklist.back();
    if (entry.isExpr()) {
      visitExpr(entry.e);
    }
    else if (entry.isUNode()) {
      visitUNode(entry.un);
    }
  }
}

void ExprInPlaceTransformer::visitExpr(Expr *e) {
  if (isa<ConstantExpr>(e)) {
    // ConstantExpr has no kids and should be omitted
    expr_kidstack.push_back((Expr*)nullptr);
    expr_worklist.pop_back();
    return;
  }
  // NonConstantExpr
  auto expr_find = visited_expr.find(e);
  if (expr_find == visited_expr.end()) {
    // 1st visit, no cached replacement result.
    // mark it as visited (under processing)
    visited_expr[e] = under_processing_expr;
    for (unsigned i=0; i < e->getNumKids(); ++i) {
      expr_worklist.push_back(e->getKid(i).get());
    }
    // need to handle updatelist in ReadExpr separately
    if (ReadExpr *RE = dyn_cast<ReadExpr>(e)) {
      expr_worklist.push_back(RE->updates.head);
    }
  }
  else if (expr_find->second == under_processing_expr) {
    // visited, no cached replacement, should pop from the worklist, rebuild itself then push to kidstack
    // kidstack looks like [ Nth_kid, ..., 1st_kid ]
    // note that update node
    //
    // Given there is no cycle, `under_processing_expr` flag works.
    ref<Expr> kids[8];
    unsigned int N = e->getNumKids();
    // Here we use std::set because different kids may be simplified to the
    // same Expr* in the end.
    std::set<Expr*> nonnull_kids;
    for (unsigned int i=0; i < N; ++i) {
        WorkListEntry &we = expr_kidstack.back();
        if (!we.isExpr()) {
          assert(0 && "rebuildInPlace expects Expr*");
        }
        kids[i] = we.e;
        if (we.e) {
          nonnull_kids.insert(we.e);
        }
        expr_kidstack.pop_back();
    }
    if (ReadExpr *RE = dyn_cast<ReadExpr>(e)) {
      // in-place generate the replacement of a ReadExpr
      // note: ReadExpr should never be omitted
      const UpdateNode *new_un = popKidUNode();
      RE->resetUpdateNode(new_un);
      if (new_un != 0 || nonnull_kids.size() != 0) {
        // Do not omit the index of last-level-read.
        // since ReadExpr only have one kid
        // (updatelist was historically not considered as kid)
        // this branch means we need rebuildInPlace (will only overwrite
        //   the index) only if:
        // 1. non-null updatelist (not last-level-read)
        // OR
        // 2. non-const index (index will not be omitted anyway)
        RE->rebuildInPlace(kids);
      }
      expr_kidstack.push_back(RE);
      visited_expr[RE] = RE;
    }
    else {
      // in-place generate the replacement of a non-ReadExpr
      Expr *replaced_expr;
      if (nonnull_kids.size() == 0) {
        // can be omitted to null
        replaced_expr = nullptr;
      }
      else if ((nonnull_kids.size() == 1) && (e->kinst == nullptr)) {
        // can be omitted to its only dependence
        replaced_expr = *(nonnull_kids.begin());
      }
      else {
        // cannot be omitted, just rebuildInPlace itself
        e->rebuildInPlace(kids);
        replaced_expr = e;
      }
      expr_kidstack.push_back(replaced_expr);
      visited_expr[e] = replaced_expr;
    }
    expr_worklist.pop_back();
  } else {
    // visited, cached replacement, should pop reuse cached replacement
    expr_kidstack.push_back(expr_find->second);
    expr_worklist.pop_back();
  }
}

void ExprInPlaceTransformer::visitUNode(const UpdateNode *un) {
  if (!un) {
    // null UNode should be shortcutted here.
    expr_kidstack.push_back((const UpdateNode*)nullptr);
    expr_worklist.pop_back();
    return;
  }
  auto un_find = visited_un.find(un);
  if (un_find == visited_un.end()) {
#ifdef EXPRINPLACE_MEMLEAK_DEBUG
    fprintf(stderr, "first time visit %p(%u)->%p\n", un, un->refCount, un->next);
#endif
    visited_un[un] = under_processing_un;
    expr_worklist.push_back(un->index.get());
    expr_worklist.push_back(un->value.get());
    expr_worklist.push_back(un->next);
  }
  else if (un_find->second == under_processing_un) {
    // kidstack from back to front: index, value, next
#ifdef EXPRINPLACE_MEMLEAK_DEBUG
    fprintf(stderr, "working on %p(%u)->%p\n", un, un->refCount, un->next);
#endif
    Expr *index = popKidExpr();
    Expr *value = popKidExpr();
    const UpdateNode *next = popKidUNode();
    if (index != un->index.get() ||
        value != un->value.get() ||
        next != un->next) {
      // this UNode need to be changed.
      if (index == nullptr && value == nullptr) {
#ifdef EXPRINPLACE_MEMLEAK_DEBUG
        fprintf(stderr, "omitted %p(%u)->%p, next %p(%u)->%p\n", un, un->refCount, un->next, next, next->refCount, next->next);
#endif
        // concrete UNode, need to be omitted
        visited_un[un] = next;
        next->inc();
        //un->dec();
        expr_kidstack.push_back(next);
      }
      else {
#ifdef EXPRINPLACE_MEMLEAK_DEBUG
        fprintf(stderr, "replaced %p(%u)->%p\n", un, un->refCount, un->next);
#endif
        // symbolic UNode, need new replacement
        UpdateNode *new_un =
          new UpdateNode(next, index, value, un->flags, un->kinst);
        visited_un[un] = new_un;
        expr_kidstack.push_back(new_un);
        // UpdateNode lifecycle management is complex. Don't touch it now.
        // FIXME: avoid UpdateNode memory leak
        // I tried and currently there is no good way to fix this.
        // I hate the way UpdateList and UpdateNode currently manage memory.
        // The problem is:
        // ExprInPlaceTransformation needs to omit and replace a single
        // UpdateNode if necessary. However, there is no way to manage a single
        // UpdateNode's allocate/free in current implementation.
        //
        // Current implementation manages memory at the UpdateList level.
        // To make things worse, not every UpdateList is in the Constraints_ty
        // passed in to ExprInPlaceTransformation.
        // There are a few UpdateList instance locate in the
        // VersionSymTabTy(std::map)
        //
        // For example, even if I know a omitted UpdateNode will never be
        // referenced again in the given constraints, I still do not know if I
        // should free it because UpdateList elsewhere may still points to it.
        //
        // Note that all commented out un->dec() is the place I should decrease
        // refCount of an UpdateNode. But I cannot do that now (otherwise there
        // will be use-after-free) due to above complaints.
        new_un->inc();
        //un->dec();
      }
    }
    else {
#ifdef EXPRINPLACE_MEMLEAK_DEBUG
      fprintf(stderr, "skipped %p(%u)->%p\n", un, un->refCount, un->next);
#endif
      // nothing changed, just return current UpdateNode itself
      // but also consider current UpdateNode visited, do not process its
      // childs again next time
      expr_kidstack.push_back(un);
      visited_un[un] = un;
    }
    expr_worklist.pop_back();
  }
  else {
    const UpdateNode *new_un = un_find->second;
#ifdef EXPRINPLACE_MEMLEAK_DEBUG
    fprintf(stderr, "processed %p(%u) dec, new is %p(%u) inc\n", un, un->refCount, new_un, new_un->refCount);
#endif
    new_un->inc();
    //un->dec();
    expr_kidstack.push_back(new_un);
    expr_worklist.pop_back();
  }
}
